// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/stereo_panner_handler.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_stereo_panner_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/stereo_panner.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

// A PannerNode only supports 1 or 2 channels
constexpr unsigned kMinimumOutputChannels = 1;
constexpr unsigned kMaximumOutputChannels = 2;

}  // namespace

StereoPannerHandler::StereoPannerHandler(AudioNode& node,
                                         float sample_rate,
                                         AudioParamHandler& pan)
    : AudioHandler(kNodeTypeStereoPanner, node, sample_rate),
      pan_(&pan),
      sample_accurate_pan_values_(
          GetDeferredTaskHandler().RenderQuantumFrames()) {
  AddInput();
  AddOutput(kMaximumOutputChannels);

  // The node-specific default mixing rules declare that StereoPannerNode
  // can handle mono to stereo and stereo to stereo conversion.
  channel_count_ = kMaximumOutputChannels;
  SetInternalChannelCountMode(V8ChannelCountMode::Enum::kClampedMax);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);

  Initialize();
}

scoped_refptr<StereoPannerHandler> StereoPannerHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& pan) {
  return base::AdoptRef(new StereoPannerHandler(node, sample_rate, pan));
}

StereoPannerHandler::~StereoPannerHandler() {
  Uninitialize();
}

void StereoPannerHandler::Process(uint32_t frames_to_process) {
  AudioBus* output_bus = Output(0).Bus();

  if (!IsInitialized() || !Input(0).IsConnected() || !stereo_panner_.get()) {
    output_bus->Zero();
    return;
  }

  scoped_refptr<AudioBus> input_bus = Input(0).Bus();
  if (!input_bus) {
    output_bus->Zero();
    return;
  }

  bool is_sample_accurate = pan_->HasSampleAccurateValues();

  if (is_sample_accurate && pan_->IsAudioRate()) {
    // Apply sample-accurate panning specified by AudioParam automation.
    DCHECK_LE(frames_to_process, sample_accurate_pan_values_.size());
    float* pan_values = sample_accurate_pan_values_.Data();
    pan_->CalculateSampleAccurateValues(pan_values, frames_to_process);
    stereo_panner_->PanWithSampleAccurateValues(input_bus.get(), output_bus,
                                                pan_values, frames_to_process);
    return;
  }

  // The pan value is not sample-accurate or not a-rate.  In this case, we have
  // a fixed pan value for the render and just need to incorporate any inputs to
  // the value, if any.
  float pan_value = is_sample_accurate ? pan_->FinalValue() : pan_->Value();

  stereo_panner_->PanToTargetValue(input_bus.get(), output_bus, pan_value,
                                   frames_to_process);
}

void StereoPannerHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  // TODO(crbug.com/40637820): Eventually, the render quantum size will no
  // longer be hardcoded as 128. At that point, we'll need to switch from
  // stack allocation to heap allocation.
  constexpr unsigned render_quantum_frames_expected = 128;
  CHECK_EQ(GetDeferredTaskHandler().RenderQuantumFrames(),
           render_quantum_frames_expected);

  float values[render_quantum_frames_expected];
  DCHECK_LE(frames_to_process, render_quantum_frames_expected);

  pan_->CalculateSampleAccurateValues(values, frames_to_process);
}

void StereoPannerHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  stereo_panner_ = std::make_unique<StereoPanner>(Context()->sampleRate());

  AudioHandler::Initialize();
}

void StereoPannerHandler::SetChannelCount(unsigned channel_count,
                                          ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  if (channel_count >= kMinimumOutputChannels &&
      channel_count <= kMaximumOutputChannels) {
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
            "channelCount", channel_count, kMinimumOutputChannels,
            ExceptionMessages::kInclusiveBound, kMaximumOutputChannels,
            ExceptionMessages::kInclusiveBound));
  }
}

void StereoPannerHandler::SetChannelCountMode(V8ChannelCountMode::Enum mode,
                                              ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  V8ChannelCountMode::Enum old_mode = InternalChannelCountMode();

  if (mode == V8ChannelCountMode::Enum::kClampedMax ||
      mode == V8ChannelCountMode::Enum::kExplicit) {
    new_channel_count_mode_ = mode;
  } else if (mode == V8ChannelCountMode::Enum::kMax) {
    // This is not supported for a StereoPannerNode, which can only handle
    // 1 or 2 channels.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "StereoPanner: 'max' is not allowed");
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
