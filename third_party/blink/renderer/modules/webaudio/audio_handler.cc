// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_handler.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

AudioHandler::AudioHandler(NodeType node_type,
                           AudioNode& node,
                           float sample_rate)
    : node_(&node),
      context_(node.context()),
      deferred_task_handler_(&context_->GetDeferredTaskHandler()) {
  SetNodeType(node_type);
  SetInternalChannelCountMode(V8ChannelCountMode::Enum::kMax);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);

#if DEBUG_AUDIONODE_REFERENCES
  if (!is_node_count_initialized_) {
    is_node_count_initialized_ = true;
    atexit(AudioHandler::PrintNodeCounts);
  }
#endif
  InstanceCounters::IncrementCounter(InstanceCounters::kAudioHandlerCounter);

  SendLogMessage(__func__, String::Format("({sample_rate=%0.f})", sample_rate));
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(
      stderr,
      "[%16p]: %16p: %2d: AudioHandler::AudioHandler() %d [%d] total: %u\n",
      Context(), this, GetNodeType(), connection_ref_count_,
      node_count_[GetNodeType()],
      InstanceCounters::CounterValue(InstanceCounters::kAudioHandlerCounter));
#endif
  node.context()->WarnIfContextClosed(this);
}

AudioHandler::~AudioHandler() {
  DCHECK(IsMainThread());
  InstanceCounters::DecrementCounter(InstanceCounters::kAudioHandlerCounter);
#if DEBUG_AUDIONODE_REFERENCES
  --node_count_[GetNodeType()];
  fprintf(
      stderr,
      "[%16p]: %16p: %2d: AudioHandler::~AudioHandler() %d [%d] remaining: "
      "%u\n",
      Context(), this, GetNodeType(), connection_ref_count_,
      node_count_[GetNodeType()],
      InstanceCounters::CounterValue(InstanceCounters::kAudioHandlerCounter));
#endif
}

void AudioHandler::Initialize() {
  DCHECK_EQ(new_channel_count_mode_, channel_count_mode_);
  DCHECK_EQ(new_channel_interpretation_, channel_interpretation_);

  is_initialized_ = true;
}

void AudioHandler::Uninitialize() {
  is_initialized_ = false;
}

void AudioHandler::Dispose() {
  DCHECK(IsMainThread());
  deferred_task_handler_->AssertGraphOwner();

  deferred_task_handler_->RemoveChangedChannelCountMode(this);
  deferred_task_handler_->RemoveChangedChannelInterpretation(this);
  deferred_task_handler_->RemoveAutomaticPullNode(this);
  for (auto& output : outputs_) {
    output->Dispose();
  }
}

AudioNode* AudioHandler::GetNode() const {
  DCHECK(IsMainThread());
  return node_;
}

BaseAudioContext* AudioHandler::Context() const {
  return context_.Get();
}

String AudioHandler::NodeTypeName() const {
  switch (node_type_) {
    case kNodeTypeDestination:
      return "AudioDestinationNode";
    case kNodeTypeOscillator:
      return "OscillatorNode";
    case kNodeTypeAudioBufferSource:
      return "AudioBufferSourceNode";
    case kNodeTypeMediaElementAudioSource:
      return "MediaElementAudioSourceNode";
    case kNodeTypeMediaStreamAudioDestination:
      return "MediaStreamAudioDestinationNode";
    case kNodeTypeMediaStreamAudioSource:
      return "MediaStreamAudioSourceNode";
    case kNodeTypeScriptProcessor:
      return "ScriptProcessorNode";
    case kNodeTypeBiquadFilter:
      return "BiquadFilterNode";
    case kNodeTypePanner:
      return "PannerNode";
    case kNodeTypeStereoPanner:
      return "StereoPannerNode";
    case kNodeTypeConvolver:
      return "ConvolverNode";
    case kNodeTypeDelay:
      return "DelayNode";
    case kNodeTypeGain:
      return "GainNode";
    case kNodeTypeChannelSplitter:
      return "ChannelSplitterNode";
    case kNodeTypeChannelMerger:
      return "ChannelMergerNode";
    case kNodeTypeAnalyser:
      return "AnalyserNode";
    case kNodeTypeDynamicsCompressor:
      return "DynamicsCompressorNode";
    case kNodeTypeWaveShaper:
      return "WaveShaperNode";
    case kNodeTypeIIRFilter:
      return "IIRFilterNode";
    case kNodeTypeConstantSource:
      return "ConstantSourceNode";
    case kNodeTypeAudioWorklet:
      return "AudioWorkletNode";
    case kNodeTypeUnknown:
    case kNodeTypeEnd:
    default:
      NOTREACHED_IN_MIGRATION();
      return "UnknownNode";
  }
}

void AudioHandler::SetNodeType(NodeType type) {
  // Don't allow the node type to be changed to a different node type, after
  // it's already been set.  And the new type can't be unknown or end.
  DCHECK_EQ(node_type_, kNodeTypeUnknown);
  DCHECK_NE(type, kNodeTypeUnknown);
  DCHECK_NE(type, kNodeTypeEnd);

  node_type_ = type;

#if DEBUG_AUDIONODE_REFERENCES
  ++node_count_[type];
  fprintf(stderr, "[%16p]: %16p: %2d: AudioHandler::AudioHandler [%3d]\n",
          Context(), this, GetNodeType(), node_count_[GetNodeType()]);
#endif
}

void AudioHandler::AddInput() {
  inputs_.push_back(std::make_unique<AudioNodeInput>(*this));
}

void AudioHandler::AddOutput(unsigned number_of_channels) {
  DCHECK(IsMainThread());

  outputs_.push_back(
      std::make_unique<AudioNodeOutput>(this, number_of_channels));
  GetNode()->DidAddOutput(NumberOfOutputs());
}

AudioNodeInput& AudioHandler::Input(unsigned i) {
  return *inputs_[i];
}

AudioNodeOutput& AudioHandler::Output(unsigned i) {
  return *outputs_[i];
}

const AudioNodeOutput& AudioHandler::Output(unsigned i) const {
  return *outputs_[i];
}

unsigned AudioHandler::ChannelCount() {
  return channel_count_;
}

void AudioHandler::SetInternalChannelCountMode(V8ChannelCountMode::Enum mode) {
  channel_count_mode_ = mode;
  new_channel_count_mode_ = mode;
}

void AudioHandler::SetInternalChannelInterpretation(
    AudioBus::ChannelInterpretation interpretation) {
  channel_interpretation_ = interpretation;
  new_channel_interpretation_ = interpretation;
}

void AudioHandler::SetChannelCount(unsigned channel_count,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  if (channel_count > 0 &&
      channel_count <= BaseAudioContext::MaxNumberOfChannels()) {
    if (channel_count_ != channel_count) {
      channel_count_ = channel_count;
      if (channel_count_mode_ != V8ChannelCountMode::Enum::kMax) {
        UpdateChannelsForInputs();
      }
    }
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<uint32_t>(
            "channel count", channel_count, 1,
            ExceptionMessages::kInclusiveBound,
            BaseAudioContext::MaxNumberOfChannels(),
            ExceptionMessages::kInclusiveBound));
  }
}

V8ChannelCountMode::Enum AudioHandler::GetChannelCountMode() {
  // Because we delay the actual setting of the mode to the pre or post
  // rendering phase, we want to return the value that was set, not the actual
  // current mode.
  return new_channel_count_mode_;
}

void AudioHandler::SetChannelCountMode(V8ChannelCountMode::Enum mode,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  new_channel_count_mode_ = mode;
  if (new_channel_count_mode_ != channel_count_mode_) {
    Context()->GetDeferredTaskHandler().AddChangedChannelCountMode(this);
  }
}

V8ChannelInterpretation::Enum AudioHandler::ChannelInterpretation() {
  // Because we delay the actual setting of the interpretation to the pre or
  // post rendering phase, we want to return the value that was set, not the
  // actual current interpretation.
  switch (new_channel_interpretation_) {
    case AudioBus::kSpeakers:
      return V8ChannelInterpretation::Enum::kSpeakers;
    case AudioBus::kDiscrete:
      return V8ChannelInterpretation::Enum::kDiscrete;
  }
  NOTREACHED();
}

void AudioHandler::SetChannelInterpretation(
    V8ChannelInterpretation::Enum interpretation,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  AudioBus::ChannelInterpretation old_mode = channel_interpretation_;

  if (interpretation == V8ChannelInterpretation::Enum::kSpeakers) {
    new_channel_interpretation_ = AudioBus::kSpeakers;
  } else if (interpretation == V8ChannelInterpretation::Enum::kDiscrete) {
    new_channel_interpretation_ = AudioBus::kDiscrete;
  } else {
    NOTREACHED();
  }

  if (new_channel_interpretation_ != old_mode) {
    Context()->GetDeferredTaskHandler().AddChangedChannelInterpretation(this);
  }
}

void AudioHandler::UpdateChannelsForInputs() {
  for (auto& input : inputs_) {
    input->ChangedOutputs();
  }
}

void AudioHandler::ProcessIfNecessary(uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());

  if (!IsInitialized()) {
    return;
  }

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "AudioHandler::ProcessIfNecessary", "this",
               reinterpret_cast<void*>(this), "node type",
               NodeTypeName().Ascii());

  // Ensure that we only process once per rendering quantum.
  // This handles the "fanout" problem where an output is connected to multiple
  // inputs.  The first time we're called during this time slice we process, but
  // after that we don't want to re-process, instead our output(s) will already
  // have the results cached in their bus;
  double current_time = Context()->currentTime();
  if (last_processing_time_ != current_time) {
    // important to first update this time because of feedback loops in the
    // rendering graph.
    last_processing_time_ = current_time;

    PullInputs(frames_to_process);

    bool silent_inputs = InputsAreSilent();
    if (silent_inputs && PropagatesSilence()) {
      SilenceOutputs();
      // AudioParams still need to be processed so that the value can be updated
      // if there are automations or so that the upstream nodes get pulled if
      // any are connected to the AudioParam.
      ProcessOnlyAudioParams(frames_to_process);
    } else {
      // Unsilence the outputs first because the processing of the node may
      // cause the outputs to go silent and we want to propagate that hint to
      // the downstream nodes.  (For example, a Gain node with a gain of 0 will
      // want to silence its output.)
      UnsilenceOutputs();
      Process(frames_to_process);
    }

    if (!silent_inputs) {
      // Update `last_non_silent_time_` AFTER processing this block.
      // Doing it before causes `PropagateSilence()` to be one render
      // quantum longer than necessary.
      last_non_silent_time_ =
          (Context()->CurrentSampleFrame() + frames_to_process) /
          static_cast<double>(Context()->sampleRate());
    }

    if (!is_processing_) {
      SendLogMessage(__func__,
                     String::Format("=> (processing is alive [frames=%u])",
                                    frames_to_process));
      is_processing_ = true;
    }
  }
}

void AudioHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  deferred_task_handler_->AssertGraphOwner();

  DCHECK(inputs_.Contains(input));

  input->UpdateInternalBus();
}

bool AudioHandler::PropagatesSilence() const {
  return last_non_silent_time_ + LatencyTime() + TailTime() <
         Context()->currentTime();
}

void AudioHandler::PullInputs(uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());

  // Process all of the AudioNodes connected to our inputs.
  for (auto& input : inputs_) {
    input->Pull(nullptr, frames_to_process);
  }
}

bool AudioHandler::InputsAreSilent() {
  for (auto& input : inputs_) {
    if (!input->Bus()->IsSilent()) {
      return false;
    }
  }
  return true;
}

void AudioHandler::SilenceOutputs() {
  for (auto& output : outputs_) {
    if (output->IsConnectedDuringRendering()) {
      output->Bus()->Zero();
    }
  }
}

void AudioHandler::UnsilenceOutputs() {
  for (auto& output : outputs_) {
    output->Bus()->ClearSilentFlag();
  }
}

void AudioHandler::EnableOutputsIfNecessary() {
  DCHECK(IsMainThread());
  deferred_task_handler_->AssertGraphOwner();

  // We're enabling outputs for this handler.  Remove this from the tail
  // processing list (if it's there) so that we don't inadvertently disable the
  // outputs later on when the tail processing time has elapsed.
  Context()->GetDeferredTaskHandler().RemoveTailProcessingHandler(this, false);

#if DEBUG_AUDIONODE_REFERENCES > 1
  fprintf(stderr,
          "[%16p]: %16p: %2d: EnableOutputsIfNecessary: is_disabled %d count "
          "%d output size %u\n",
          Context(), this, GetNodeType(), is_disabled_, connection_ref_count_,
          outputs_.size());
#endif

  if (is_disabled_ && connection_ref_count_ > 0) {
    is_disabled_ = false;
    for (auto& output : outputs_) {
      output->Enable();
    }
  }
}

void AudioHandler::DisableOutputsIfNecessary() {
  // This function calls other functions that require graph ownership,
  // so assert that this needs graph ownership too.
  deferred_task_handler_->AssertGraphOwner();

#if DEBUG_AUDIONODE_REFERENCES > 1
  fprintf(stderr,
          "[%16p]: %16p: %2d: DisableOutputsIfNecessary is_disabled %d count %d"
          " tail %d\n",
          Context(), this, GetNodeType(), is_disabled_, connection_ref_count_,
          RequiresTailProcessing());
#endif

  // Disable outputs if appropriate. We do this if the number of connections is
  // 0 or 1. The case of 0 is from deref() where there are no connections left.
  // The case of 1 is from AudioNodeInput::disable() where we want to disable
  // outputs when there's only one connection left because we're ready to go
  // away, but can't quite yet.
  if (connection_ref_count_ <= 1 && !is_disabled_) {
    // Still may have JavaScript references, but no more "active" connection
    // references, so put all of our outputs in a "dormant" disabled state.
    // Garbage collection may take a very long time after this time, so the
    // "dormant" disabled nodes should not bog down the rendering...

    // As far as JavaScript is concerned, our outputs must still appear to be
    // connected.  But internally our outputs should be disabled from the inputs
    // they're connected to.  disable() can recursively deref connections (and
    // call disable()) down a whole chain of connected nodes.

    // If a node requires tail processing, we defer the disabling of
    // the outputs so that the tail for the node can be output.
    // Otherwise, we can disable the outputs right away.
    if (RequiresTailProcessing()) {
      if (deferred_task_handler_->AcceptsTailProcessing()) {
        deferred_task_handler_->AddTailProcessingHandler(this);
      }
    } else {
      DisableOutputs();
    }
  }
}

void AudioHandler::DisableOutputs() {
  is_disabled_ = true;
  for (auto& output : outputs_) {
    output->Disable();
  }
}

void AudioHandler::MakeConnection() {
  deferred_task_handler_->AssertGraphOwner();
  connection_ref_count_++;

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(
      stderr,
      "[%16p]: %16p: %2d: AudioHandler::MakeConnection   %3d [%3d] @%.15g\n",
      Context(), this, GetNodeType(), connection_ref_count_,
      node_count_[GetNodeType()], Context()->currentTime());
#endif

  // See the disabling code in disableOutputsIfNecessary(). This handles
  // the case where a node is being re-connected after being used at least
  // once and disconnected. In this case, we need to re-enable.
  EnableOutputsIfNecessary();
}

void AudioHandler::BreakConnectionWithLock() {
  deferred_task_handler_->AssertGraphOwner();
  connection_ref_count_--;

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr,
          "[%16p]: %16p: %2d: AudioHandler::BreakConnectionWitLock %3d [%3d] "
          "@%.15g\n",
          Context(), this, GetNodeType(), connection_ref_count_,
          node_count_[GetNodeType()], Context()->currentTime());
#endif

  if (!connection_ref_count_) {
    DisableOutputsIfNecessary();
  }
}

#if DEBUG_AUDIONODE_REFERENCES

bool AudioHandler::is_node_count_initialized_ = false;
int AudioHandler::node_count_[kNodeTypeEnd];

void AudioHandler::PrintNodeCounts() {
  fprintf(stderr, "\n\n");
  fprintf(stderr, "===========================\n");
  fprintf(stderr, "AudioNode: reference counts\n");
  fprintf(stderr, "===========================\n");

  for (unsigned i = 0; i < kNodeTypeEnd; ++i)
    fprintf(stderr, "%2d: %d\n", i, node_count_[i]);

  fprintf(stderr, "===========================\n\n\n");
}

#endif  // DEBUG_AUDIONODE_REFERENCES

#if DEBUG_AUDIONODE_REFERENCES > 1
void AudioHandler::TailProcessingDebug(const char* note, bool flag) {
  fprintf(stderr, "[%16p]: %16p: %2d: %s %d @%.15g flag=%d", Context(), this,
          GetNodeType(), note, connection_ref_count_, Context()->currentTime(),
          flag);

  // If we're on the audio thread, we can print out the tail and
  // latency times (because these methods can only be called from the
  // audio thread.)
  if (Context()->IsAudioThread()) {
    fprintf(stderr, ", tail=%.15g + %.15g, last=%.15g\n", TailTime(),
            LatencyTime(), last_non_silent_time_);
  }

  fprintf(stderr, "\n");
}

void AudioHandler::AddTailProcessingDebug() {
  TailProcessingDebug("addTail", false);
}

void AudioHandler::RemoveTailProcessingDebug(bool disable_outputs) {
  TailProcessingDebug("remTail", disable_outputs);
}
#endif  // DEBUG_AUDIONODE_REFERENCES > 1

void AudioHandler::UpdateChannelCountMode() {
  channel_count_mode_ = new_channel_count_mode_;
  UpdateChannelsForInputs();
}

void AudioHandler::UpdateChannelInterpretation() {
  channel_interpretation_ = new_channel_interpretation_;
}

unsigned AudioHandler::NumberOfOutputChannels() const {
  // This should only be called for ScriptProcessorNodes which are the only
  // nodes where you can have an output with 0 channels.  All other nodes have
  // have at least one output channel, so there's no reason other nodes should
  // ever call this function.
  DCHECK(0) << "numberOfOutputChannels() not valid for node type "
            << GetNodeType();
  return 1;
}

void AudioHandler::SendLogMessage(const char* const function_name,
                                  const String& message) {
  WebRtcLogMessage(String::Format("[WA]AH::%s %s [type=%s, this=0x%" PRIXPTR
                                  "]",
                                  function_name, message.Utf8().c_str(),
                                  NodeTypeName().Utf8().c_str(),
                                  reinterpret_cast<uintptr_t>(this))
                       .Utf8());
}

}  // namespace blink
