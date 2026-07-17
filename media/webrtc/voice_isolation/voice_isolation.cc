// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include <memory>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/converting_audio_fifo.h"
#include "media/webrtc/voice_isolation/passthrough_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {

namespace {
constexpr int kInternalFrameSize = 320;
constexpr int kInternalFramesPerSecond = 50;

std::unique_ptr<VoiceIsolationComponent> CreateVoiceIsolation() {
  return std::make_unique<PassthroughVoiceIsolation>(kInternalFrameSize,
                                                     kInternalFramesPerSecond);
}
}  // namespace

std::unique_ptr<VoiceIsolation> VoiceIsolation::Create(
    const tflite::FlatBufferModel* model,
    const media::AudioParameters& audio_params) {
  // TODO(barrerap): Pass the model to VoiceIsolation once it is supported.
  std::unique_ptr<VoiceIsolationComponent> component = CreateVoiceIsolation();

  return base::WrapUnique(
      new VoiceIsolation(std::move(component), audio_params));
}

VoiceIsolation::VoiceIsolation(
    std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation,
    const media::AudioParameters& audio_params)
    : internal_voice_isolation_(std::move(internal_voice_isolation)) {
  CHECK(audio_params.IsValid());

  media::AudioParameters mono_internal(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(),
      kInternalFrameSize * kInternalFramesPerSecond, kInternalFrameSize);

  forward_fifo_ =
      std::make_unique<ConvertingAudioFifo>(audio_params, mono_internal);
  backward_fifo_ =
      std::make_unique<ConvertingAudioFifo>(mono_internal, audio_params);
}

VoiceIsolation::~VoiceIsolation() = default;

void VoiceIsolation::ProcessAudio(const AudioBus& input_bus,
                                  AudioBus& output_bus) {
  CHECK_EQ(input_bus.frames(), output_bus.frames());
  CHECK_EQ(input_bus.channels(), output_bus.channels());

  // We cannot pass `input_bus` directly because we only hold a const reference
  // and ConvertingAudioFifo::Push takes ownership (std::unique_ptr<AudioBus>).
  auto input_copy =
      media::AudioBus::Create(input_bus.channels(), input_bus.frames());
  input_bus.CopyTo(input_copy.get());

  forward_fifo_->Push(std::move(input_copy));

  while (forward_fifo_->HasOutput()) {
    const media::AudioBus* internal_in = forward_fifo_->PeekOutput();
    auto internal_out = media::AudioBus::Create(1, internal_in->frames());

    internal_voice_isolation_->ProcessAudio(internal_in->channel(0),
                                            internal_out->channel(0));

    forward_fifo_->PopOutput();
    backward_fifo_->Push(std::move(internal_out));
  }

  if (backward_fifo_->HasOutput()) {
    const media::AudioBus* out = backward_fifo_->PeekOutput();
    out->CopyTo(&output_bus);
    backward_fifo_->PopOutput();
  } else {
    output_bus.Zero();
  }
}

}  // namespace media
