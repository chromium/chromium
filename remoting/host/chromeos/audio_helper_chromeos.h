// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_AUDIO_HELPER_CHROMEOS_H_
#define REMOTING_HOST_CHROMEOS_AUDIO_HELPER_CHROMEOS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/base/desktop_environment_options.h"

namespace remoting {

class AudioPacket;

// AudioHelperChromeOs is designed to live on the AudioManager's task runner.
// It handles all interactions with the media::AudioInputStream and implements
// the AudioInputCallback callback interface. It is intended to be owned and
// managed via base::SequenceBound from AudioCapturerChromeOs on the main
// thread.
class AudioHelperChromeOs {
 public:
  using OnDataCallback =
      base::RepeatingCallback<void(std::unique_ptr<AudioPacket>)>;
  using OnErrorCallback = base::RepeatingCallback<void()>;

  virtual ~AudioHelperChromeOs() = default;

  // Methods to be called on the AudioManager's sequence.
  virtual void StartAudioStream(AudioPlaybackMode audio_playback_mode,
                                OnDataCallback on_data_callback,
                                OnErrorCallback on_error_callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_AUDIO_HELPER_CHROMEOS_H_
