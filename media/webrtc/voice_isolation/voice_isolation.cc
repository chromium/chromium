// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/channel_mixer.h"
#include "media/webrtc/voice_isolation/band_split_voice_isolation.h"
#include "media/webrtc/voice_isolation/buffered_voice_isolation.h"
#include "media/webrtc/voice_isolation/stft_voice_isolation.h"
#include "media/webrtc/voice_isolation/tflite_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {

namespace {
// StftVoiceIsolation consumes one 20 ms waveform frame per two DFTs.
constexpr size_t kStftFrameSize =
    BandSplitVoiceIsolation::kFrameSize / BandSplitVoiceIsolation::kNumDfts;
constexpr size_t kStftFramesPerSecond =
    BandSplitVoiceIsolation::kFramesPerSecond;

// External frames: 10 ms at 48 kHz, as delivered by APM. BufferedVoiceIsolation
// combines two of them into one STFT frame.
constexpr size_t kExternalFramesPerStftFrame =
    BufferedVoiceIsolation::kNumBufferedFrames;
constexpr size_t kVoiceIsolationFramesPerSecond =
    kStftFramesPerSecond * kExternalFramesPerStftFrame;
static_assert(VoiceIsolation::kFrameSize ==
              kStftFrameSize / kExternalFramesPerStftFrame);
static_assert(VoiceIsolation::kFrameSize * kVoiceIsolationFramesPerSecond ==
              VoiceIsolation::kSampleRate);

base::expected<std::unique_ptr<VoiceIsolationComponent>,
               VoiceIsolationCreationResult>
CreateVoiceIsolation(const tflite::FlatBufferModel* model) {
  CHECK(model);

  // Create the TfLite inference component (expects two 16kHz DFTs of 160
  // complex bins each).
  ASSIGN_OR_RETURN(std::unique_ptr<VoiceIsolationComponent> tflite,
                   TfLiteVoiceIsolation::MaybeCreate(model));

  // Wrap TfLite with BandSplitVoiceIsolation, which CHECKs the model layout,
  // to split 48kHz DFTs down to 16kHz and zero-pad high bands on
  // reconstruction. StftVoiceIsolation performs the 48kHz STFT and iSTFT on
  // 20 ms frames, and BufferedVoiceIsolation lets the pipeline consume the
  // 10 ms frames delivered by APM. The VoiceIsolationImpl constructor CHECKs
  // the layout of the returned component.
  return std::make_unique<BufferedVoiceIsolation>(
      std::make_unique<StftVoiceIsolation>(
          std::make_unique<BandSplitVoiceIsolation>(std::move(tflite))));
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
  void ClearBuffers() override;
  base::TimeDelta AlgorithmicDelay() const override;

 private:
  // Mono component that processes exactly one external buffer per call.
  const std::unique_ptr<VoiceIsolationComponent> voice_isolation_component_;

  // Cached algorithmic delay of `voice_isolation_component_`.
  const base::TimeDelta algorithmic_delay_;

  // Channel count of the external buffers.
  const int channels_;

  // Downmix the external layout to mono and upmix the result back. Built once,
  // off the audio thread. Null when the external layout is already mono.
  std::unique_ptr<ChannelMixer> downmixer_;
  std::unique_ptr<ChannelMixer> upmixer_;

  // Preallocated mono scratch buses, only used when mixing. Keeping them as
  // members avoids allocations on the real-time audio thread. Null when the
  // external layout is already mono.
  std::unique_ptr<AudioBus> mono_input_bus_;
  std::unique_ptr<AudioBus> mono_output_bus_;
};

VoiceIsolationImpl::VoiceIsolationImpl(
    std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation,
    const media::AudioParameters& audio_params)
    : voice_isolation_component_(std::move(internal_voice_isolation)),
      algorithmic_delay_(voice_isolation_component_
                             ? voice_isolation_component_->AlgorithmicDelay()
                             : base::TimeDelta()),
      channels_(audio_params.channels()) {
  CHECK(voice_isolation_component_);
  CHECK(VoiceIsolation::SupportsAudioParameters(audio_params));

  // There is no FIFO to rebuffer mismatched sizes: every external buffer maps
  // to one mono component call of `kFrameSize` samples,
  // `kVoiceIsolationFramesPerSecond` times per second. Create() accepts any
  // component, so CHECK that it matches this exact frame layout.
  CHECK_EQ(voice_isolation_component_->FrameSize(), kFrameSize);
  CHECK_EQ(voice_isolation_component_->FramesPerSecond(),
           kVoiceIsolationFramesPerSecond);

  // Mono buffers feed the component directly, so no mixing is needed.
  if (channels_ == 1) {
    return;
  }

  // Build the mixing matrices and scratch buses once, off the audio thread.
  const ChannelLayoutConfig& external_layout =
      audio_params.channel_layout_config();
  downmixer_ = std::make_unique<ChannelMixer>(external_layout,
                                              ChannelLayoutConfig::Mono());
  upmixer_ = std::make_unique<ChannelMixer>(ChannelLayoutConfig::Mono(),
                                            external_layout);
  mono_input_bus_ = AudioBus::Create(1, audio_params.frames_per_buffer());
  mono_output_bus_ = AudioBus::Create(1, audio_params.frames_per_buffer());
}

VoiceIsolationImpl::~VoiceIsolationImpl() = default;

void VoiceIsolationImpl::ProcessAudio(const AudioBus& input_bus,
                                      AudioBus& output_bus) {
  TRACE_EVENT("audio", "VoiceIsolationImpl::ProcessAudio");

  // Enforce the documented contract: two different buses, both with the
  // channel count and the 10 ms buffer size passed to Create().
  CHECK_NE(&input_bus, &output_bus);
  CHECK_EQ(input_bus.frames(), output_bus.frames());
  CHECK_EQ(static_cast<size_t>(input_bus.frames()), kFrameSize);
  CHECK_EQ(input_bus.channels(), output_bus.channels());
  CHECK_EQ(input_bus.channels(), channels_);

  if (channels_ == 1) {
    voice_isolation_component_->ProcessAudio(input_bus.channel(0),
                                             output_bus.channel(0));
    return;
  }

  downmixer_->Transform(&input_bus, mono_input_bus_.get());
  voice_isolation_component_->ProcessAudio(mono_input_bus_->channel(0),
                                           mono_output_bus_->channel(0));
  upmixer_->Transform(mono_output_bus_.get(), &output_bus);
}

void VoiceIsolationImpl::ClearBuffers() {
  // The scratch buses are fully overwritten on every call, so only the
  // component holds state.
  voice_isolation_component_->ClearBuffers();
}

base::TimeDelta VoiceIsolationImpl::AlgorithmicDelay() const {
  return algorithmic_delay_;
}
}  // namespace

bool VoiceIsolation::SupportsAudioParameters(
    const media::AudioParameters& audio_params) {
  return audio_params.IsValid() && audio_params.sample_rate() == kSampleRate &&
         static_cast<size_t>(audio_params.frames_per_buffer()) == kFrameSize;
}

base::expected<std::unique_ptr<VoiceIsolationComponent>,
               VoiceIsolationCreationResult>
VoiceIsolation::CreateComponent(const tflite::FlatBufferModel* model) {
  return CreateVoiceIsolation(model);
}

std::unique_ptr<VoiceIsolation> VoiceIsolation::Create(
    std::unique_ptr<VoiceIsolationComponent> component,
    const media::AudioParameters& audio_params) {
  CHECK(component);
  return std::make_unique<VoiceIsolationImpl>(std::move(component),
                                              audio_params);
}

std::unique_ptr<VoiceIsolation> VoiceIsolation::Create(
    const tflite::FlatBufferModel* model,
    const media::AudioParameters& audio_params) {
  auto component_or_error = CreateComponent(model);
  return component_or_error.has_value()
             ? Create(std::move(*component_or_error), audio_params)
             : nullptr;
}

}  // namespace media
