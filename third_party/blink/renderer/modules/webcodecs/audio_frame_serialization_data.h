// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_SERIALIZATION_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_SERIALIZATION_DATA_H_

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// Wrapper that contains all the necessary information to recreate an
// AudioFrame. It abstracts how audio data is actually backed, to simplify
// lifetime guarantees when jumping threads.
// TODO(https://crbug.com/1168418): add actual serialization support, to allow
// the use of AudioFrames in workers.
class MODULES_EXPORT AudioFrameSerializationData {
 public:
  virtual ~AudioFrameSerializationData() = default;

  AudioFrameSerializationData(const AudioFrameSerializationData&) = delete;
  AudioFrameSerializationData& operator=(const AudioFrameSerializationData&) =
      delete;

  // Helper function that creates a simple media::AudioBus backed wrapper.
  static std::unique_ptr<AudioFrameSerializationData> Wrap(
      std::unique_ptr<media::AudioBus> data,
      int sample_rate,
      base::TimeDelta timestamp);

  virtual media::AudioBus* data() = 0;

  int sample_rate() const { return sample_rate_; }

  base::TimeDelta timestamp() const { return timestamp_; }

 protected:
  AudioFrameSerializationData(int sample_rate, base::TimeDelta timestamp);

 private:
  int sample_rate_;
  base::TimeDelta timestamp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_SERIALIZATION_DATA_H_
