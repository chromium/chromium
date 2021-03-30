// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"

namespace blink {

namespace {
class BasicAudioFrameSerializationData final
    : public AudioFrameSerializationData {
 public:
  BasicAudioFrameSerializationData(std::unique_ptr<media::AudioBus> data,
                                   int sample_rate,
                                   base::TimeDelta timestamp)
      : AudioFrameSerializationData(sample_rate, timestamp),
        data_(std::move(data)) {}
  ~BasicAudioFrameSerializationData() override = default;

  media::AudioBus* data() override { return data_.get(); }

 private:
  std::unique_ptr<media::AudioBus> data_;
};
}  // namespace

AudioFrameSerializationData::AudioFrameSerializationData(
    int sample_rate,
    base::TimeDelta timestamp)
    : sample_rate_(sample_rate), timestamp_(timestamp) {}

// static
std::unique_ptr<AudioFrameSerializationData> AudioFrameSerializationData::Wrap(
    std::unique_ptr<media::AudioBus> data,
    int sample_rate,
    base::TimeDelta timestamp) {
  return std::make_unique<BasicAudioFrameSerializationData>(
      std::move(data), sample_rate, timestamp);
}

}  // namespace blink
