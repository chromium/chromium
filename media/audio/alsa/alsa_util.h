// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ALSA_ALSA_UTIL_H_
#define MEDIA_AUDIO_ALSA_ALSA_UTIL_H_

#include <alsa/asoundlib.h>
#include <string>

#include "media/base/media_export.h"

namespace media {
class AlsaWrapper;
}

namespace alsa_util {

// When opening ALSA devices, |period_us| is the size of a packet and
// |buffer_us| is the size of the ring buffer, which consists of multiple
// packets. In capture devices, the latency relies more on |period_us|, and thus
// one may require more details upon the value implicitly set by ALSA.
MEDIA_EXPORT snd_pcm_t* OpenCaptureDevice(media::AlsaWrapper* wrapper,
                                          const char* device_name,
                                          int channels,
                                          int sample_rate,
                                          snd_pcm_format_t pcm_format,
                                          int buffer_us,
                                          int period_us);

snd_pcm_t* OpenPlaybackDevice(media::AlsaWrapper* wrapper,
                              const char* device_name,
                              int channels,
                              int sample_rate,
                              snd_pcm_format_t pcm_format,
                              int buffer_us);

int CloseDevice(media::AlsaWrapper* wrapper, snd_pcm_t* handle);

snd_mixer_t* OpenMixer(media::AlsaWrapper* wrapper,
                       const std::string& device_name);

void CloseMixer(media::AlsaWrapper* wrapper,
                snd_mixer_t* mixer,
                const std::string& device_name);

snd_mixer_elem_t* LoadCaptureMixerElement(media::AlsaWrapper* wrapper,
                                          snd_mixer_t* mixer);

}  // namespace alsa_util

#endif  // MEDIA_AUDIO_ALSA_ALSA_UTIL_H_
