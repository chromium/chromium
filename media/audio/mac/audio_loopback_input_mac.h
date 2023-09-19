// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_LOOPBACK_INPUT_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_LOOPBACK_INPUT_MAC_H_

#include "media/audio/mac/audio_manager_mac.h"
#include "media/base/audio_parameters.h"

namespace media {

// ScreenCaptureKit uses the default sample rate of 48kHz.
static constexpr uint32_t kLoopbackSampleRate = 48000;

// Documentation for the AudioInputStream implementation in
// audio_loopback_input_mac.h.
// Returns a nullptr if the API is unavailable.
// Supported sample rates: 8000Hz, 16000Hz, 24000Hz, 48000Hz.
// Supported channel layouts: mono, stereo.
AudioInputStream* CreateSCKAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    AudioManager::LogCallback log_callback,
    const base::RepeatingCallback<void(AudioInputStream*)> close_callback);

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_LOOPBACK_INPUT_MAC_H_
