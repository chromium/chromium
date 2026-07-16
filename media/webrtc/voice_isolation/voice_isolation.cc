// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include <memory>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/voice_isolation/passthrough_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {

namespace {
constexpr int kInternalFrameSize = 480;
constexpr int kInternalFramesPerSecond = 100;

std::unique_ptr<VoiceIsolationComponent> CreateVoiceIsolation() {
  return std::make_unique<PassthroughVoiceIsolation>(kInternalFrameSize,
                                                     kInternalFramesPerSecond);
}

bool IsFormatSupported(const media::AudioParameters& audio_params) {
  return (audio_params.format() ==
              media::AudioParameters::Format::AUDIO_PCM_LINEAR ||
          audio_params.format() ==
              media::AudioParameters::Format::AUDIO_PCM_LOW_LATENCY ||
          audio_params.format() == media::AudioParameters::Format::AUDIO_FAKE);
}
}  // namespace

std::unique_ptr<VoiceIsolation> VoiceIsolation::Create(
    const tflite::FlatBufferModel* model,
    const media::AudioParameters& audio_params) {
  CHECK(IsFormatSupported(audio_params));

  // TODO(barrerap): Pass the model to VoiceIsolation once it is supported.
  std::unique_ptr<VoiceIsolationComponent> component = CreateVoiceIsolation();

  return base::WrapUnique(new VoiceIsolation(std::move(component)));
}

// TODO(crbug.com/40176497): False positive in presubmit, not detecting
// correctly this method as test-only just by its name.
std::unique_ptr<VoiceIsolation> VoiceIsolation::CreateForTesting(  // IN-TEST
    const media::AudioParameters& audio_params) {
  CHECK(IsFormatSupported(audio_params));

  std::unique_ptr<VoiceIsolationComponent> component =
      std::make_unique<PassthroughVoiceIsolation>(kInternalFrameSize,
                                                  kInternalFramesPerSecond);
  return base::WrapUnique(new VoiceIsolation(std::move(component)));
}

VoiceIsolation::VoiceIsolation(
    std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation)
    : internal_voice_isolation_(std::move(internal_voice_isolation)) {}

VoiceIsolation::~VoiceIsolation() = default;

void VoiceIsolation::ProcessAudio(const AudioBus& input_bus,
                                  AudioBus& output_bus) {
  CHECK_EQ(input_bus.channel(0).size(), output_bus.channel(0).size());
  media::AudioBus::ConstChannel input_channel = input_bus.channel(0);
  media::AudioBus::Channel output_channel = output_bus.channel(0);

  internal_voice_isolation_->ProcessAudio(input_channel, output_channel);

  // `internal_voice_isolation_->ProcessAudio()` only processes the first
  // channel (mono). This for loop copies the first channel to all channels to
  // provide fake multi-channel support.
  for (auto channel : base::span(output_bus.AllChannels()).subspan(1u)) {
    channel.copy_from_nonoverlapping(output_channel);
  }
}

}  // namespace media
