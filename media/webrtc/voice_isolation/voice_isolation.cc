// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include <memory>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/converting_audio_fifo.h"
#include "media/webrtc/voice_isolation/passthrough_voice_isolation.h"
#include "media/webrtc/voice_isolation/stft_voice_isolation.h"
#include "media/webrtc/voice_isolation/tflite_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {

namespace {
constexpr size_t kVoiceIsolationFrameSize = 320;
constexpr size_t kVoiceIsolationFramesPerSecond = 50;

std::unique_ptr<VoiceIsolationComponent> CreateVoiceIsolation(
    const tflite::FlatBufferModel* model) {
  // Internally the model expects two sets of complex coefficients of two DFT of
  // 160 samples.
  constexpr size_t kModelFrameSize = 2 * kVoiceIsolationFrameSize;
  CHECK(model);

  std::unique_ptr<VoiceIsolationComponent> tflite =
      TfLiteVoiceIsolation::MaybeCreate(model);
  // TODO(barrerap): We are assuming the model is always correct. This is
  // because VoiceIsolationHandler, the caller to `VoiceIsolation::Create`,
  // expects that we always are able to create a valid. In the future we will
  // handle both incorrect initializations and delayed initializations
  // (`TfLiteVoiceIsolation::MaybeCreate` might be slow).
  CHECK(tflite);
  CHECK_EQ(tflite->FrameSize(), 640u);
  CHECK_EQ(tflite->FramesPerSecond(), kVoiceIsolationFramesPerSecond);

  auto stft = std::make_unique<StftVoiceIsolation>(std::move(tflite));
  CHECK_EQ(stft->FrameSize(), kModelFrameSize / 2);
  CHECK_EQ(stft->FramesPerSecond(), kVoiceIsolationFramesPerSecond);
  return stft;
}

class VoiceIsolationImpl : public VoiceIsolation {
 public:
  VoiceIsolationImpl(
      std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation,
      const media::AudioParameters& audio_params);
  ~VoiceIsolationImpl() override;

  VoiceIsolationImpl(const VoiceIsolationImpl&) = delete;
  VoiceIsolationImpl& operator=(const VoiceIsolationImpl&) = delete;

  void ProcessAudio(const AudioBus& input_bus, AudioBus& output_bus) override;

 private:
  std::unique_ptr<VoiceIsolationComponent> voice_isolation_component_;
  std::unique_ptr<ConvertingAudioFifo> forward_fifo_;
  std::unique_ptr<ConvertingAudioFifo> backward_fifo_;
};

VoiceIsolationImpl::VoiceIsolationImpl(
    std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation,
    const media::AudioParameters& audio_params)
    : voice_isolation_component_(std::move(internal_voice_isolation)) {
  CHECK(audio_params.IsValid());

  media::AudioParameters mono_internal(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(),
      kVoiceIsolationFrameSize * kVoiceIsolationFramesPerSecond,
      kVoiceIsolationFrameSize);

  forward_fifo_ =
      std::make_unique<ConvertingAudioFifo>(audio_params, mono_internal,
                                            /*use_input_bus_pool=*/true);
  backward_fifo_ =
      std::make_unique<ConvertingAudioFifo>(mono_internal, audio_params,
                                            /*use_input_bus_pool=*/true);
}

VoiceIsolationImpl::~VoiceIsolationImpl() = default;

void VoiceIsolationImpl::ProcessAudio(const AudioBus& input_bus,
                                      AudioBus& output_bus) {
  TRACE_EVENT("audio", "VoiceIsolationImpl::ProcessAudio");
  CHECK_EQ(input_bus.frames(), output_bus.frames());
  CHECK_EQ(input_bus.channels(), output_bus.channels());

  // We cannot pass `input_bus` directly because we only hold a const reference
  // and ConvertingAudioFifo::Push takes ownership (std::unique_ptr<AudioBus>).
  // Instead we use a AudioBus from the internal pool of the `forward_fifo_` and
  // `backward_fifo_`. Calls from `ProcessAudio()` will only require memory
  // allocation in the first few calls.
  std::unique_ptr<AudioBus> input_copy = forward_fifo_->GetInputAudioBus();
  CHECK(input_copy);
  input_bus.CopyTo(input_copy.get());

  forward_fifo_->Push(std::move(input_copy));

  while (forward_fifo_->HasOutput()) {
    TRACE_EVENT("audio", "VoiceIsolationImpl::ProcessInternalFrame");
    const media::AudioBus* internal_in = forward_fifo_->PeekOutput();
    std::unique_ptr<media::AudioBus> internal_out =
        backward_fifo_->GetInputAudioBus();

    voice_isolation_component_->ProcessAudio(internal_in->channel(0),
                                            internal_out->channel(0));

    forward_fifo_->PopOutput();
    backward_fifo_->Push(std::move(internal_out));
  }

  if (backward_fifo_->HasOutput()) {
    const media::AudioBus* out = backward_fifo_->PeekOutput();
    out->CopyTo(&output_bus);
    backward_fifo_->PopOutput();
  } else {
    TRACE_EVENT_INSTANT("audio", "VoiceIsolationImpl::OutputZeroed");
    output_bus.Zero();
  }
}
}  // namespace

std::unique_ptr<VoiceIsolation> VoiceIsolation::Create(
    const tflite::FlatBufferModel* model,
    const media::AudioParameters& audio_params) {
  std::unique_ptr<VoiceIsolationComponent> component =
      CreateVoiceIsolation(model);

  return std::make_unique<VoiceIsolationImpl>(std::move(component),
                                              audio_params);
}

}  // namespace media
