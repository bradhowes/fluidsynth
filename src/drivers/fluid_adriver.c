/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include "fluid_adriver.h"
#include "fluid_sys.h"
#include "fluid_settings.h"

/*
 * fluid_adriver_definition_t
 */

struct _fluid_audriver_definition_t
{
    const char *name;
    fluid_audio_driver_t *(*new)(fluid_settings_t *settings, fluid_synth_t *synth);
    fluid_audio_driver_t *(*new2)(fluid_settings_t *settings,
                                  fluid_audio_func_t func,
                                  void *data);
    void (*free)(fluid_audio_driver_t *driver);
    void (*settings)(fluid_settings_t *settings);
};

/* Available audio drivers, listed in order of preference */
static const fluid_audriver_definition_t fluid_audio_drivers[] =
{
#if JACK_SUPPORT
    {
        "jack_client",
        new_fluid_jack_audio_driver,
        new_fluid_jack_audio_driver2,
        delete_fluid_jack_audio_driver,
        fluid_jack_audio_driver_settings
    },
#endif

#if ALSA_SUPPORT
    {
        "alsa",
        new_fluid_alsa_audio_driver,
        new_fluid_alsa_audio_driver2,
        delete_fluid_alsa_audio_driver,
        fluid_alsa_audio_driver_settings
    },
#endif

#if PULSE_SUPPORT
    {
        "pulseaudio",
        new_fluid_pulse_audio_driver,
        new_fluid_pulse_audio_driver2,
        delete_fluid_pulse_audio_driver,
        fluid_pulse_audio_driver_settings
    },
#endif

#if OSS_SUPPORT
    {
        "oss",
        new_fluid_oss_audio_driver,
        new_fluid_oss_audio_driver2,
        delete_fluid_oss_audio_driver,
        fluid_oss_audio_driver_settings
    },
#endif

#if JACK_SUPPORT
    {
        "jack",
        new_fluid_jack_audio_driver_server,
        new_fluid_jack_audio_driver_server2,
        delete_fluid_jack_audio_driver,
        fluid_jack_audio_driver_settings
    },
#endif

#if OBOE_SUPPORT
    {
        "oboe",
        new_fluid_oboe_audio_driver,
        NULL,
        delete_fluid_oboe_audio_driver,
        fluid_oboe_audio_driver_settings
    },
#endif

#if OPENSLES_SUPPORT
    {
        "opensles",
        new_fluid_opensles_audio_driver,
        NULL,
        delete_fluid_opensles_audio_driver,
        fluid_opensles_audio_driver_settings
    },
#endif

#if COREAUDIO_SUPPORT
    {
        "coreaudio",
        new_fluid_core_audio_driver,
        new_fluid_core_audio_driver2,
        delete_fluid_core_audio_driver,
        fluid_core_audio_driver_settings
    },
#endif

#if DSOUND_SUPPORT
    {
        "dsound",
        new_fluid_dsound_audio_driver,
        new_fluid_dsound_audio_driver2,
        delete_fluid_dsound_audio_driver,
        fluid_dsound_audio_driver_settings
    },
#endif

#if WASAPI_SUPPORT
    {
        "wasapi",
        new_fluid_wasapi_audio_driver,
        new_fluid_wasapi_audio_driver2,
        delete_fluid_wasapi_audio_driver,
        fluid_wasapi_audio_driver_settings
    },
#endif

#if WAVEOUT_SUPPORT
    {
        "waveout",
        new_fluid_waveout_audio_driver,
        new_fluid_waveout_audio_driver2,
        delete_fluid_waveout_audio_driver,
        fluid_waveout_audio_driver_settings
    },
#endif

#if SNDMAN_SUPPORT
    {
        "sndman",
        new_fluid_sndmgr_audio_driver,
        new_fluid_sndmgr_audio_driver2,
        delete_fluid_sndmgr_audio_driver,
        NULL
    },
#endif

#if PORTAUDIO_SUPPORT
    {
        "portaudio",
        new_fluid_portaudio_driver,
        NULL,
        delete_fluid_portaudio_driver,
        fluid_portaudio_driver_settings
    },
#endif

#if DART_SUPPORT
    {
        "dart",
        new_fluid_dart_audio_driver,
        NULL,
        delete_fluid_dart_audio_driver,
        fluid_dart_audio_driver_settings
    },
#endif

#if SDL2_SUPPORT
    {
        "sdl2",
        new_fluid_sdl2_audio_driver,
        NULL,
        delete_fluid_sdl2_audio_driver,
        fluid_sdl2_audio_driver_settings
    },
#endif

#if AUFILE_SUPPORT
    {
        "file",
        new_fluid_file_audio_driver,
        NULL,
        delete_fluid_file_audio_driver,
        NULL
    },
#endif
    /* NULL terminator to avoid zero size array if no driver available */
    { NULL, NULL, NULL, NULL, NULL }
};

#define ENABLE_AUDIO_DRIVER(_drv, _idx) \
    _drv[(_idx) / (sizeof(*(_drv))*8)] &= ~(1 << ((_idx) % (sizeof((*_drv))*8)))

#define IS_AUDIO_DRIVER_ENABLED(_drv, _idx) \
    (!(_drv[(_idx) / (sizeof(*(_drv))*8)] & (1 << ((_idx) % (sizeof((*_drv))*8)))))

static uint8_t fluid_adriver_disable_mask[(FLUID_N_ELEMENTS(fluid_audio_drivers) + 7) / 8] = {0};

void fluid_audio_driver_settings(fluid_settings_t *settings)
{
    unsigned int i;

    fluid_settings_register_str(settings, "audio.sample-format", "16bits", 0);
    fluid_settings_add_option(settings, "audio.sample-format", "16bits");
    fluid_settings_add_option(settings, "audio.sample-format", "float");

#if defined(WIN32)
    fluid_settings_register_int(settings, "audio.period-size", 512, 64, 8192, 0);
    fluid_settings_register_int(settings, "audio.periods", 8, 2, 64, 0);
#elif defined(MACOS9)
    fluid_settings_register_int(settings, "audio.period-size", 64, 64, 8192, 0);
    fluid_settings_register_int(settings, "audio.periods", 8, 2, 64, 0);
#else
    fluid_settings_register_int(settings, "audio.period-size", 64, 64, 8192, 0);
    fluid_settings_register_int(settings, "audio.periods", 16, 2, 64, 0);
#endif

    fluid_settings_register_int(settings, "audio.realtime-prio",
                                FLUID_DEFAULT_AUDIO_RT_PRIO, 0, 99, 0);

    fluid_settings_register_str(settings, "audio.driver", "", 0);

    for(i = 0; i < FLUID_N_ELEMENTS(fluid_audio_drivers) - 1; i++)
    {
        /* Add the driver to the list of options */
        fluid_settings_add_option(settings, "audio.driver", fluid_audio_drivers[i].name);

        if(fluid_audio_drivers[i].settings != NULL &&
                IS_AUDIO_DRIVER_ENABLED(fluid_adriver_disable_mask, i))
        {
            fluid_audio_drivers[i].settings(settings);
        }
    }

    fluid_settings_setstr(settings, "audio.driver", "auto");
}

static fluid_audio_driver_t *
create_fluid_audio_driver(fluid_settings_t *settings, fluid_synth_t *synth,
        fluid_audio_func_t func, void *data)
{
    unsigned int i;
    char *valid_options;
    char *selected_option = NULL;
    fluid_audio_driver_t *driver = NULL;
    const fluid_audriver_definition_t *def;
    int auto_select = fluid_settings_str_equal(settings, "audio.driver", "auto");

    for(i = 0; i < FLUID_N_ELEMENTS(fluid_audio_drivers) - 1; i++)
    {
        if(!IS_AUDIO_DRIVER_ENABLED(fluid_adriver_disable_mask, i))
        {
            continue;
        }

        def = &fluid_audio_drivers[i];

        if (!auto_select && !fluid_settings_str_equal(settings, "audio.driver", def->name))
        {
            continue;
        }

        FLUID_LOG(FLUID_DBG, "Trying '%s' audio driver", def->name);

        if (func == NULL)
        {
            driver = (*def->new)(settings, synth);
        }
        else
        {
            if (def->new2 == NULL)
            {
                FLUID_LOG(FLUID_DBG, "'%s' audio driver does not support callback mode", def->name);
                driver = NULL;
            }
            else
            {
                driver = (*def->new2)(settings, func, data);
            }
        }

        if (driver)
        {
            driver->define = def;
            return driver;
        }

        FLUID_LOG(FLUID_DBG, "'%s' audio driver failed to start", def->name);

        if (auto_select)
        {
            continue;
        }

        break;
    }

    valid_options = fluid_settings_option_concat(settings, "audio.driver", NULL);
    if (valid_options == NULL || valid_options[0] == '\0')
    {
        FLUID_FREE(valid_options);
        FLUID_LOG(FLUID_ERR, "No audio drivers available.");
        return NULL;
    }

    if (auto_select)
    {
        FLUID_LOG(FLUID_ERR,
                "Couldn't auto-select an audio driver, tried %s",
                valid_options);
    }
    else
    {
        fluid_settings_dupstr(settings, "audio.driver", &selected_option);
        if (fluid_settings_option_is_valid(settings, "audio.driver", selected_option))
        {
            FLUID_LOG(FLUID_ERR, "Couldn't start the requested audio driver '%s'.",
                    selected_option ? selected_option : "NULL");
        }
        else
        {
            FLUID_LOG(FLUID_ERR, "Invalid audio driver '%s'.",
                    selected_option ? selected_option : "NULL");
            FLUID_LOG(FLUID_INFO, "Valid audio drivers are: %s", valid_options);
        }
        FLUID_FREE(selected_option);
    }

    FLUID_FREE(valid_options);

    return NULL;
}

/**
 * Create a new audio driver.
 *
 * @param settings Configuration settings used to select and create the audio
 *   driver.
 * @param synth Synthesizer instance for which the audio driver is created for.
 * @return The new audio driver instance or NULL on error
 *
 * Creates a new audio driver for a given \p synth instance with a defined set
 * of configuration \p settings. The \p settings instance must be the same that
 * you have passed to new_fluid_synth() when creating the \p synth instance.
 * Otherwise the behaviour is undefined.
 *
 * @note As soon as an audio driver is created, the \p synth starts rendering audio.
 * This means that all necessary sound-setup should be completed after this point,
 * thus of all object types in use (synth, midi player, sequencer, etc.) the audio
 * driver should always be the last one to be created and the first one to be deleted!
 * Also refer to the order of object creation in the code examples.
 */
fluid_audio_driver_t *
new_fluid_audio_driver(fluid_settings_t *settings, fluid_synth_t *synth)
{
    return create_fluid_audio_driver(settings, synth, NULL, NULL);
}

/**
 * Create a new audio driver.
 *
 * @param settings Configuration settings used to select and create the audio
 *   driver.
 * @param func Function called to fill audio buffers for audio playback
 * @param data User defined data pointer to pass to \p func
 * @return The new audio driver instance or NULL on error
 *
 * Like new_fluid_audio_driver() but allows for custom audio processing before
 * audio is sent to audio driver. It is the responsibility of the callback
 * \p func to render the audio into the buffers. If \p func uses a fluid_synth_t \p synth,
 * the \p settings instance must be the same that you have passed to new_fluid_synth()
 * when creating the \p synth instance. Otherwise the behaviour is undefined.
 *
 * @note Not as efficient as new_fluid_audio_driver().
 *
 * @note As soon as an audio driver is created, a new thread is spawned starting to make
 * callbacks to \p func.
 * This means that all necessary sound-setup should be completed after this point,
 * thus of all object types in use (synth, midi player, sequencer, etc.) the audio
 * driver should always be the last one to be created and the first one to be deleted!
 * Also refer to the order of object creation in the code examples.
 */
fluid_audio_driver_t *
new_fluid_audio_driver2(fluid_settings_t *settings, fluid_audio_func_t func, void *data)
{
    return create_fluid_audio_driver(settings, NULL, func, data);
}

/**
 * Deletes an audio driver instance.
 *
 * @param driver Audio driver instance to delete
 *
 * Shuts down an audio driver and deletes its instance.
 */
void
delete_fluid_audio_driver(fluid_audio_driver_t *driver)
{
    fluid_return_if_fail(driver != NULL);
    driver->define->free(driver);
}


/**
 * Registers audio drivers to use
 *
 * @param adrivers NULL-terminated array of audio drivers to register. Pass NULL to register all available drivers.
 * @return #FLUID_OK if all the audio drivers requested by the user are supported by fluidsynth and have been
 * successfully registered. Otherwise #FLUID_FAILED is returned and this function has no effect.
 *
 * When creating a settings instance with new_fluid_settings(), all audio drivers are initialized once.
 * In the past this has caused segfaults and application crashes due to buggy soundcard drivers.
 *
 * This function enables the user to only initialize specific audio drivers when settings instances are created.
 * Therefore pass a NULL-terminated array of C-strings containing the \c names of audio drivers to register
 * for the usage with fluidsynth.
 * The \c names are the same as being used for the \c audio.driver setting.
 *
 * By default all audio drivers fluidsynth has been compiled with are registered, so calling this function is optional.
 *
 * @warning This function may only be called if no thread is residing in fluidsynth's API and no instances of any kind
 * are alive (e.g. as it would be the case right after fluidsynth's initial creation). Else the behaviour is undefined.
 * Furtermore any attempt of using audio drivers that have not been registered is undefined behaviour!
 *
 * @note This function is not thread safe and will never be!
 *
 * @since 1.1.9
 */
int fluid_audio_driver_register(const char **adrivers)
{
    unsigned int i;
    uint8_t      disable_mask[FLUID_N_ELEMENTS(fluid_adriver_disable_mask)];

    if(adrivers == NULL)
    {
        /* Pass NULL to register all available drivers. */
        FLUID_MEMSET(fluid_adriver_disable_mask, 0, sizeof(fluid_adriver_disable_mask));

        return FLUID_OK;
    }

    FLUID_MEMSET(disable_mask, 0xFF, sizeof(disable_mask));

    for(i = 0; adrivers[i] != NULL; i++)
    {
        unsigned int j;

        /* search the requested audio driver in the template and enable it if found */
        for(j = 0; j < FLUID_N_ELEMENTS(fluid_audio_drivers) - 1; j++)
        {
            if(FLUID_STRCMP(adrivers[i], fluid_audio_drivers[j].name) == 0)
            {
                ENABLE_AUDIO_DRIVER(disable_mask, j);
                break;
            }
        }

        if(j >= FLUID_N_ELEMENTS(fluid_audio_drivers) - 1)
        {
            /* requested driver not found, failure */
            return FLUID_FAILED;
        }
    }

    /* Update list of activated drivers */
    FLUID_MEMCPY(fluid_adriver_disable_mask, disable_mask, sizeof(disable_mask));

    return FLUID_OK;
}
