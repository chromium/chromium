// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/dynamics_compressor_handler.h"

#include "base/trace_event/typed_macros.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_dynamics_compressor_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/dynamics_compressor.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

// Set output to stereo by default.
constexpr unsigned kDefaultNumberOfOutputChannels = 2;

}  // namespace

DynamicsCompressorHandler::DynamicsCompressorHandler(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& threshold,
    AudioParamHandler& knee,
    AudioParamHandler& ratio,
    AudioParamHandler& attack,
    AudioParamHandler& release)
    : AudioHandler(kNodeTypeDynamicsCompressor, node, sample_rate),
      threshold_(&threshold),
      knee_(&knee),
      ratio_(&ratio),
      reduction_(0),
      attack_(&attack),
      release_(&release) {
  AddInput();
  AddOutput(kDefaultNumberOfOutputChannels);

  SetInternalChannelCountMode(V8ChannelCountMode::Enum::kClampedMax);

  Initialize();
}

scoped_refptr<DynamicsCompressorHandler> DynamicsCompressorHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& threshold,
    AudioParamHandler& knee,
    AudioParamHandler& ratio,
    AudioParamHandler& attack,
    AudioParamHandler& release) {
  return base::AdoptRef(new DynamicsCompressorHandler(
      node, sample_rate, threshold, knee, ratio, attack, release));
}

DynamicsCompressorHandler::~DynamicsCompressorHandler() {
  Uninitialize();
}

void DynamicsCompressorHandler::Process(uint32_t frames_to_process) {
  float threshold = threshold_->FinalValue();
  float knee = knee_->FinalValue();
  float ratio = ratio_->FinalValue();
  float attack = attack_->FinalValue();
  float release = release_->FinalValue();

  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
              "DynamicsCompressorHandler::Process", "this",
              reinterpret_cast<void*>(this), "threshold", threshold, "knee",
              knee, "ratio", ratio, "attack", attack, "release", release);

  AudioBus* output_bus = Output(0).Bus();
  DCHECK(output_bus);

  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamThreshold,
                                          threshold);
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamKnee, knee);
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamRatio,
                                          ratio);
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamAttack,
                                          attack);
  dynamics_compressor_->SetParameterValue(DynamicsCompressor::kParamRelease,
                                          release);

  scoped_refptr<AudioBus> input_bus = Input(0).Bus();
  dynamics_compressor_->Process(input_bus.get(), output_bus, frames_to_process);

  reduction_.store(
      dynamics_compressor_->ParameterValue(DynamicsCompressor::kParamReduction),
      std::memory_order_relaxed);
}

void DynamicsCompressorHandler::ProcessOnlyAudioParams(
    uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());
  // TODO(crbug.com/40637820): Eventually, the render quantum size will no
  // longer be hardcoded as 128. At that point, we'll need to switch from
  // stack allocation to heap allocation.
  constexpr unsigned render_quantum_frames_expected = 128;
  CHECK_EQ(GetDeferredTaskHandler().RenderQuantumFrames(),
           render_quantum_frames_expected);
  DCHECK_LE(frames_to_process, render_quantum_frames_expected);

  float values[render_quantum_frames_expected];

  threshold_->CalculateSampleAccurateValues(values, frames_to_process);
  knee_->CalculateSampleAccurateValues(values, frames_to_process);
  ratio_->CalculateSampleAccurateValues(values, frames_to_process);
  attack_->CalculateSampleAccurateValues(values, frames_to_process);
  release_->CalculateSampleAccurateValues(values, frames_to_process);
}

void DynamicsCompressorHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  AudioHandler::Initialize();
  dynamics_compressor_ = std::make_unique<DynamicsCompressor>(
      Context()->sampleRate(), kDefaultNumberOfOutputChannels);
}

bool DynamicsCompressorHandler::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both be zero.
  return true;
}

double DynamicsCompressorHandler::TailTime() const {
  return dynamics_compressor_->TailTime();
}

double DynamicsCompressorHandler::LatencyTime() const {
  return dynamics_compressor_->LatencyTime();
}

void DynamicsCompressorHandler::SetChannelCount(
    unsigned channel_count,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  // A DynamicsCompressorNode only supports 1 or 2 channels
  if (channel_count > 0 && channel_count <= 2) {
    if (channel_count_ != channel_count) {
      channel_count_ = channel_count;
      if (InternalChannelCountMode() != V8ChannelCountMode::Enum::kMax) {
        UpdateChannelsForInputs();
      }
    }
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<uint32_t>(
            "channelCount", channel_count, 1,
            ExceptionMessages::kInclusiveBound, 2,
            ExceptionMessages::kInclusiveBound));
  }
}

void DynamicsCompressorHandler::SetChannelCountMode(
    V8ChannelCountMode::Enum mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  V8ChannelCountMode::Enum old_mode = InternalChannelCountMode();

  if (mode == V8ChannelCountMode::Enum::kClampedMax ||
      mode == V8ChannelCountMode::Enum::kExplicit) {
    new_channel_count_mode_ = mode;
  } else if (mode == V8ChannelCountMode::Enum::kMax) {
    // This is not supported for a DynamicsCompressorNode, which can
    // only handle 1 or 2 channels.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The provided value 'max' is not an "
                                      "allowed value for ChannelCountMode");
    new_channel_count_mode_ = old_mode;
  } else {
    // Do nothing for other invalid values.
    new_channel_count_mode_ = old_mode;
  }

  if (new_channel_count_mode_ != old_mode) {
    Context()->GetDeferredTaskHandler().AddChangedChannelCountMode(this);
  }
}

}  // namespace blink
