// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_processor_handler.h"

#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"

namespace audio {

AudioProcessorHandler::AudioProcessorHandler(
    const media::AudioProcessingSettings& settings,
    mojo::PendingReceiver<media::mojom::AudioProcessorControls>
        controls_receiver)
    : receiver_(this, std::move(controls_receiver)) {
  DCHECK(settings.NeedAudioModification());
}

AudioProcessorHandler::~AudioProcessorHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
}

void AudioProcessorHandler::OnPlayoutData(const media::AudioBus& audio_bus,
                                          int sample_rate,
                                          base::TimeDelta delay) {
  TRACE_EVENT2("audio", "AudioProcessorHandler::OnPlayoutData", " this ",
               static_cast<void*>(this), "delay", delay.InMillisecondsF());
  // TODO(https://crbug.com/1215061): Forward playout data to audio processor.
}
}  // namespace audio
