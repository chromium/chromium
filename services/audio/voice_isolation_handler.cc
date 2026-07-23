// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/voice_isolation_handler.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/base/audio_bus.h"
#include "media/webrtc/ml_model_handle.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"
#include "services/audio/ml_model_manager.h"

namespace audio {

VoiceIsolationHandler::VoiceIsolationHandler(
    scoped_refptr<media::MlModelHandle> model_handle,
    const media::AudioParameters& output_params,
    DeliverProcessedAudioCallback deliver_processed_audio_callback)
    : model_handle_(std::move(model_handle)),
      voice_isolation_(
          media::VoiceIsolation::Create(&model_handle_->Get(), output_params)),
      deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)),
      output_bus_(media::AudioBus::Create(output_params)) {
  CHECK(!deliver_processed_audio_callback_.is_null());
  CHECK(output_bus_);
}

VoiceIsolationHandler::~VoiceIsolationHandler() = default;

void VoiceIsolationHandler::ProcessCapturedAudio(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    std::optional<double> volume,
    const media::AudioGlitchInfo& audio_glitch_info) {
  DCHECK_EQ(output_bus_->channels(), audio_source.channels());
  DCHECK_EQ(output_bus_->frames(), audio_source.frames());
  voice_isolation_->ProcessAudio(audio_source, *output_bus_);
  deliver_processed_audio_callback_.Run(*output_bus_, audio_capture_time,
                                        volume, audio_glitch_info);
}

std::unique_ptr<VoiceIsolationHandler> VoiceIsolationHandler::MaybeCreate(
    MlModelManager& ml_model_manager,
    const media::AudioParameters& output_params,
    DeliverProcessedAudioCallback deliver_processed_audio_callback) {
  scoped_refptr<media::MlModelHandle> model_handle =
      ml_model_manager.GetModel(mojom::MlModelType::kVoiceIsolationDenoiser);

  if (!model_handle) {
    // Model not available or there is a problem with the model manager.
    return nullptr;
  }

  return base::WrapUnique(
      new VoiceIsolationHandler(std::move(model_handle), output_params,
                                std::move(deliver_processed_audio_callback)));
}
}  // namespace audio
