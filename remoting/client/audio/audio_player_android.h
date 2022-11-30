// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_AUDIO_PLAYER_ANDROID_H_
#define REMOTING_CLIENT_AUDIO_AUDIO_PLAYER_ANDROID_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "base/memory/weak_ptr.h"
#include "remoting/client/audio/audio_player.h"

namespace remoting {

// TODO(nicholss): Update legacy audio player to use new audio buffer code.

class AudioPlayerAndroid : public AudioPlayer {
 public:
  AudioPlayerAndroid();

  AudioPlayerAndroid(const AudioPlayerAndroid&) = delete;
  AudioPlayerAndroid& operator=(const AudioPlayerAndroid&) = delete;

  ~AudioPlayerAndroid() override;

  base::WeakPtr<AudioPlayerAndroid> GetWeakPtr();

  // AudioPlayer overrides.
  uint32_t GetSamplesPerFrame() override;
  bool ResetAudioPlayer(AudioPacket::SamplingRate sampling_rate) override;

 private:
  // Called when new data is needed for the buffer queue.
  static void BufferQueueCallback(SLAndroidSimpleBufferQueueItf caller,
                                  void* args);
  static SLDataFormat_PCM CreatePcmFormat(int sampling_rate);

  // Destroys the player and releases the buffer. Do nothing if the player is
  // nullptr.
  void DestroyPlayer();

  SLObjectItf engine_object_ = nullptr;
  SLEngineItf engine_ = nullptr;
  SLObjectItf output_mix_object_ = nullptr;
  SLObjectItf player_object_ = nullptr;
  SLPlayItf player_ = nullptr;
  SLAndroidSimpleBufferQueueItf buffer_queue_ = nullptr;
  std::unique_ptr<uint8_t[]> frame_buffer_;
  size_t buffer_size_ = 0;
  uint32_t sample_per_frame_ = 0;

  base::WeakPtrFactory<AudioPlayerAndroid> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_AUDIO_PLAYER_ANDROID_H_
