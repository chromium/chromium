// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_AUDIO_PROCESSOR_HANDLER_H_
#define SERVICES_AUDIO_AUDIO_PROCESSOR_HANDLER_H_

#include "base/sequence_checker.h"
#include "media/base/audio_processing.h"
#include "services/audio/reference_output.h"

namespace media {
class AudioBus;
}  // namespace media

namespace audio {

// Encapsulates audio processing effects in the audio process.
// TODO(https://crbug.com/1215061): Create and manage a media::AudioProcessor.
// This class is currently a no-op implementation of ReferenceOutput::Listener.
class AudioProcessorHandler final : public ReferenceOutput::Listener {
 public:
  explicit AudioProcessorHandler(
      const media::AudioProcessingSettings& settings);
  AudioProcessorHandler(const AudioProcessorHandler&) = delete;
  AudioProcessorHandler& operator=(const AudioProcessorHandler&) = delete;
  ~AudioProcessorHandler() final;

 private:
  // ReferenceOutput::Listener implementation.
  void OnPlayoutData(const media::AudioBus& audio_bus,
                     int sample_rate,
                     base::TimeDelta delay) final;

  SEQUENCE_CHECKER(owning_sequence_);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_AUDIO_PROCESSOR_HANDLER_H_
