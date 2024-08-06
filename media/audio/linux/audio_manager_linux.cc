// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/chromeos_buildflags.h"
#include "media/audio/fake_audio_manager.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"

#if defined(USE_ALSA)
#include "media/audio/alsa/audio_manager_alsa.h"
#endif

#if BUILDFLAG(USE_CRAS)
#include "media/audio/cras/audio_manager_cras.h"
#endif

#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/audio_manager_pulse.h"
#include "media/audio/pulse/pulse_util.h"
#endif

namespace media {

std::unique_ptr<media::AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  // For testing allow audio output to be disabled.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioOutput)) {
    return std::make_unique<FakeAudioManager>(std::move(audio_thread),
                                              audio_log_factory);
  }

#if BUILDFLAG(USE_CRAS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseCras)) {
    return std::make_unique<AudioManagerCras>(std::move(audio_thread),
                                              audio_log_factory);
  }
#endif  // BUILDFLAG(USE_CRAS)

#if defined(USE_PULSEAUDIO)
  pa_threaded_mainloop* pa_mainloop = nullptr;
  pa_context* pa_context = nullptr;
  if (pulse::InitPulse(&pa_mainloop, &pa_context)) {
    return std::make_unique<AudioManagerPulse>(
        std::move(audio_thread), audio_log_factory, pa_mainloop, pa_context);
  }
  LOG(WARNING) << "Falling back to ALSA for audio output. PulseAudio is not "
                  "available or could not be initialized.";
#endif

#if defined(USE_ALSA)
  return std::make_unique<AudioManagerAlsa>(std::move(audio_thread),
                                            audio_log_factory);
#else
  return std::make_unique<FakeAudioManager>(std::move(audio_thread),
                                            audio_log_factory);
#endif
}

}  // namespace media
