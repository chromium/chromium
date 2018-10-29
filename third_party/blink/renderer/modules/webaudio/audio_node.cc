/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instance_counters.h"
#include "third_party/blink/renderer/platform/wtf/atomics.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

AudioHandler::AudioHandler(NodeType node_type,
                           AudioNode& node,
                           float sample_rate)
    : is_initialized_(false),
      node_type_(kNodeTypeUnknown),
      node_(&node),
      context_(node.context()),
      last_processing_time_(-1),
      last_non_silent_time_(0),
      connection_ref_count_(0),
      is_disabled_(false),
      channel_count_(2) {
  SetNodeType(node_type);
  SetInternalChannelCountMode(kMax);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);

#if DEBUG_AUDIONODE_REFERENCES
  if (!is_node_count_initialized_) {
    is_node_count_initialized_ = true;
    atexit(AudioHandler::PrintNodeCounts);
  }
#endif
  InstanceCounters::IncrementCounter(InstanceCounters::kAudioHandlerCounter);

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(
      stderr,
      "[%16p]: %16p: %2d: AudioHandler::AudioHandler() %d [%d] total: %u\n",
      Context(), this, GetNodeType(), connection_ref_count_,
      node_count_[GetNodeType()],
      InstanceCounters::CounterValue(InstanceCounters::kAudioHandlerCounter));
#endif
}

AudioHandler::~AudioHandler() {
  DCHECK(IsMainThread());
  // dispose() should be called.
  DCHECK(!GetNode());
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
  Context()->AssertGraphOwner();

  Context()->GetDeferredTaskHandler().RemoveChangedChannelCountMode(this);
  Context()->GetDeferredTaskHandler().RemoveChangedChannelInterpretation(this);
  Context()->GetDeferredTaskHandler().RemoveAutomaticPullNode(this);
  for (auto& output : outputs_)
    output->Dispose();
  node_ = nullptr;
}

AudioNode* AudioHandler::GetNode() const {
  DCHECK(IsMainThread());
  return node_;
}

BaseAudioContext* AudioHandler::Context() const {
  return context_;
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
    case kNodeTypeUnknown:
    case kNodeTypeEnd:
    default:
      NOTREACHED();
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
  inputs_.push_back(AudioNodeInput::Create(*this));
}

void AudioHandler::AddOutput(unsigned number_of_channels) {
  DCHECK(IsMainThread());
  outputs_.push_back(AudioNodeOutput::Create(this, number_of_channels));
  GetNode()->DidAddOutput(NumberOfOutputs());
}

AudioNodeInput& AudioHandler::Input(unsigned i) {
  return *inputs_[i];
}

AudioNodeOutput& AudioHandler::Output(unsigned i) {
  return *outputs_[i];
}

unsigned long AudioHandler::ChannelCount() {
  return channel_count_;
}

void AudioHandler::SetInternalChannelCountMode(ChannelCountMode mode) {
  channel_count_mode_ = mode;
  new_channel_count_mode_ = mode;
}

void AudioHandler::SetInternalChannelInterpretation(
    AudioBus::ChannelInterpretation interpretation) {
  channel_interpretation_ = interpretation;
  new_channel_interpretation_ = interpretation;
}

void AudioHandler::SetChannelCount(unsigned long channel_count,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  if (channel_count > 0 &&
      channel_count <= BaseAudioContext::MaxNumberOfChannels()) {
    if (channel_count_ != channel_count) {
      channel_count_ = channel_count;
      if (channel_count_mode_ != kMax)
        UpdateChannelsForInputs();
    }
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<unsigned long>(
            "channel count", channel_count, 1,
            ExceptionMessages::kInclusiveBound,
            BaseAudioContext::MaxNumberOfChannels(),
            ExceptionMessages::kInclusiveBound));
  }
}

String AudioHandler::GetChannelCountMode() {
  // Because we delay the actual setting of the mode to the pre or post
  // rendering phase, we want to return the value that was set, not the actual
  // current mode.
  switch (new_channel_count_mode_) {
    case kMax:
      return "max";
    case kClampedMax:
      return "clamped-max";
    case kExplicit:
      return "explicit";
  }
  NOTREACHED();
  return "";
}

void AudioHandler::SetChannelCountMode(const String& mode,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  ChannelCountMode old_mode = channel_count_mode_;

  if (mode == "max") {
    new_channel_count_mode_ = kMax;
  } else if (mode == "clamped-max") {
    new_channel_count_mode_ = kClampedMax;
  } else if (mode == "explicit") {
    new_channel_count_mode_ = kExplicit;
  } else {
    NOTREACHED();
  }

  if (new_channel_count_mode_ != old_mode)
    Context()->GetDeferredTaskHandler().AddChangedChannelCountMode(this);
}

String AudioHandler::ChannelInterpretation() {
  // Because we delay the actual setting of the interpreation to the pre or
  // post rendering phase, we want to return the value that was set, not the
  // actual current interpretation.
  switch (new_channel_interpretation_) {
    case AudioBus::kSpeakers:
      return "speakers";
    case AudioBus::kDiscrete:
      return "discrete";
  }
  NOTREACHED();
  return "";
}

void AudioHandler::SetChannelInterpretation(const String& interpretation,
                                            ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  AudioBus::ChannelInterpretation old_mode = channel_interpretation_;

  if (interpretation == "speakers") {
    new_channel_interpretation_ = AudioBus::kSpeakers;
  } else if (interpretation == "discrete") {
    new_channel_interpretation_ = AudioBus::kDiscrete;
  } else {
    NOTREACHED();
  }

  if (new_channel_interpretation_ != old_mode)
    Context()->GetDeferredTaskHandler().AddChangedChannelInterpretation(this);
}

void AudioHandler::UpdateChannelsForInputs() {
  for (auto& input : inputs_)
    input->ChangedOutputs();
}

void AudioHandler::ProcessIfNecessary(size_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());

  if (!IsInitialized())
    return;

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
      // Update |last_non_silent_time| AFTER processing this block.
      // Doing it before causes |PropagateSilence()| to be one render
      // quantum longer than necessary.
      last_non_silent_time_ =
          (Context()->CurrentSampleFrame() + frames_to_process) /
          static_cast<double>(Context()->sampleRate());
    }
  }
}

void AudioHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK(inputs_.Contains(input));
  if (!inputs_.Contains(input))
    return;

  input->UpdateInternalBus();
}

bool AudioHandler::PropagatesSilence() const {
  return last_non_silent_time_ + LatencyTime() + TailTime() <
         Context()->currentTime();
}

void AudioHandler::PullInputs(size_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());

  // Process all of the AudioNodes connected to our inputs.
  for (auto& input : inputs_)
    input->Pull(nullptr, frames_to_process);
}

bool AudioHandler::InputsAreSilent() {
  for (auto& input : inputs_) {
    if (!input->Bus()->IsSilent())
      return false;
  }
  return true;
}

void AudioHandler::SilenceOutputs() {
  for (auto& output : outputs_)
    output->Bus()->Zero();
}

void AudioHandler::UnsilenceOutputs() {
  for (auto& output : outputs_)
    output->Bus()->ClearSilentFlag();
}

void AudioHandler::EnableOutputsIfNecessary() {
  DCHECK(IsMainThread());
  Context()->AssertGraphOwner();

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
    for (auto& output : outputs_)
      output->Enable();
  }
}

void AudioHandler::DisableOutputsIfNecessary() {
  // This function calls other functions that require graph ownership,
  // so assert that this needs graph ownership too.
  Context()->AssertGraphOwner();

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
      if (Context()->ContextState() !=
          BaseAudioContext::AudioContextState::kClosed) {
        Context()->GetDeferredTaskHandler().AddTailProcessingHandler(this);
      }
    } else {
      DisableOutputs();
    }
  }
}

void AudioHandler::DisableOutputs() {
  is_disabled_ = true;
  for (auto& output : outputs_)
    output->Disable();
}

void AudioHandler::MakeConnection() {
  AtomicIncrement(&connection_ref_count_);

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

void AudioHandler::BreakConnection() {
  // The actual work for deref happens completely within the audio context's
  // graph lock. In the case of the audio thread, we must use a tryLock to
  // avoid glitches.
  bool has_lock = false;
  if (Context()->IsAudioThread()) {
    // Real-time audio thread must not contend lock (to avoid glitches).
    has_lock = Context()->TryLock();
  } else {
    Context()->lock();
    has_lock = true;
  }

  if (has_lock) {
    BreakConnectionWithLock();
    Context()->unlock();
  } else {
    // We were unable to get the lock, so put this in a list to finish up
    // later.
    DCHECK(Context()->IsAudioThread());
    Context()->GetDeferredTaskHandler().AddDeferredBreakConnection(*this);
  }
}

void AudioHandler::BreakConnectionWithLock() {
  Context()->AssertGraphOwner();

  AtomicDecrement(&connection_ref_count_);

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr,
          "[%16p]: %16p: %2d: AudioHandler::BreakConnectionWitLock %3d [%3d] "
          "@%.15g\n",
          Context(), this, GetNodeType(), connection_ref_count_,
          node_count_[GetNodeType()], Context()->currentTime());
#endif

  if (!connection_ref_count_)
    DisableOutputsIfNecessary();
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
// ----------------------------------------------------------------

AudioNode::AudioNode(BaseAudioContext& context)
    : context_(context),
      deferred_task_handler_(&context.GetDeferredTaskHandler()),
      handler_(nullptr) {}

AudioNode::~AudioNode() {
  // The graph lock is required to destroy the handler. And we can't use
  // |context_| to touch it, since that object may also be a dead heap object.
  {
    DeferredTaskHandler::GraphAutoLocker locker(*deferred_task_handler_);
    handler_ = nullptr;
  }
}

void AudioNode::Dispose() {
  DCHECK(IsMainThread());
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: %16p: %2d: AudioNode::dispose %16p @%g\n", context(),
          this, Handler().GetNodeType(), handler_.get(),
          context()->currentTime());
#endif
  BaseAudioContext::GraphAutoLocker locker(context());
  Handler().Dispose();

  if (context()->HasRealtimeConstraint()) {
    // Add the handler to the orphan list if the context is not
    // closed. (Nothing will clean up the orphan list if the context
    // is closed.)  These will get cleaned up in the post render task
    // if audio thread is running or when the context is colleced (in
    // the worst case).
    if (context()->ContextState() != BaseAudioContext::kClosed) {
      context()->GetDeferredTaskHandler().AddRenderingOrphanHandler(
          std::move(handler_));
    }
  } else {
    // For an offline context, only need to save the handler when the
    // context is running.  The change in the context state is
    // synchronous with the main thread (even though the offline
    // thread is not synchronized to the main thread).
    if (context()->ContextState() == BaseAudioContext::kRunning) {
      context()->GetDeferredTaskHandler().AddRenderingOrphanHandler(
          std::move(handler_));
    }
  }
}

void AudioNode::SetHandler(scoped_refptr<AudioHandler> handler) {
  DCHECK(handler);
  handler_ = std::move(handler);

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: %16p: %2d: AudioNode::AudioNode %16p\n", context(),
          this, handler_->GetNodeType(), handler_.get());
#endif
}

AudioHandler& AudioNode::Handler() const {
  return *handler_;
}

void AudioNode::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  visitor->Trace(connected_nodes_);
  visitor->Trace(connected_params_);
  EventTargetWithInlineData::Trace(visitor);
}

void AudioNode::HandleChannelOptions(const AudioNodeOptions& options,
                                     ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (options.hasChannelCount())
    setChannelCount(options.channelCount(), exception_state);
  if (options.hasChannelCountMode())
    setChannelCountMode(options.channelCountMode(), exception_state);
  if (options.hasChannelInterpretation())
    setChannelInterpretation(options.channelInterpretation(), exception_state);
}

BaseAudioContext* AudioNode::context() const {
  return context_;
}

AudioNode* AudioNode::connect(AudioNode* destination,
                              unsigned output_index,
                              unsigned input_index,
                              ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(context());

  if (context()->IsContextClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot connect after the context has been closed.");
    return nullptr;
  }

  if (!destination) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "invalid destination node.");
    return nullptr;
  }

  // Sanity check input and output indices.
  if (output_index >= numberOfOutputs()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "output index (" + String::Number(output_index) +
            ") exceeds number of outputs (" +
            String::Number(numberOfOutputs()) + ").");
    return nullptr;
  }

  if (destination && input_index >= destination->numberOfInputs()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "input index (" + String::Number(input_index) +
            ") exceeds number of inputs (" +
            String::Number(destination->numberOfInputs()) + ").");
    return nullptr;
  }

  if (context() != destination->context()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "cannot connect to a destination "
        "belonging to a different audio context.");
    return nullptr;
  }

  // ScriptProcessorNodes with 0 output channels can't be connected to any
  // destination.  If there are no output channels, what would the destination
  // receive?  Just disallow this.
  if (Handler().GetNodeType() == AudioHandler::kNodeTypeScriptProcessor &&
      Handler().NumberOfOutputChannels() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "cannot connect a ScriptProcessorNode "
                                      "with 0 output channels to any "
                                      "destination node.");
    return nullptr;
  }

  destination->Handler()
      .Input(input_index)
      .Connect(Handler().Output(output_index));
  if (!connected_nodes_[output_index])
    connected_nodes_[output_index] = new HeapHashSet<Member<AudioNode>>();
  connected_nodes_[output_index]->insert(destination);

  Handler().UpdatePullStatusIfNeeded();

  return destination;
}

void AudioNode::connect(AudioParam* param,
                        unsigned output_index,
                        ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(context());

  if (context()->IsContextClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot connect after the context has been closed.");
    return;
  }

  if (!param) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "invalid AudioParam.");
    return;
  }

  if (output_index >= numberOfOutputs()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "output index (" + String::Number(output_index) +
            ") exceeds number of outputs (" +
            String::Number(numberOfOutputs()) + ").");
    return;
  }

  if (context() != param->Context()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "cannot connect to an AudioParam "
        "belonging to a different audio context.");
    return;
  }

  param->Handler().Connect(Handler().Output(output_index));
  if (!connected_params_[output_index])
    connected_params_[output_index] = new HeapHashSet<Member<AudioParam>>();
  connected_params_[output_index]->insert(param);

  Handler().UpdatePullStatusIfNeeded();
}

void AudioNode::DisconnectAllFromOutput(unsigned output_index) {
  Handler().Output(output_index).DisconnectAll();
  connected_nodes_[output_index] = nullptr;
  connected_params_[output_index] = nullptr;
}

bool AudioNode::DisconnectFromOutputIfConnected(
    unsigned output_index,
    AudioNode& destination,
    unsigned input_index_of_destination) {
  AudioNodeOutput& output = Handler().Output(output_index);
  AudioNodeInput& input =
      destination.Handler().Input(input_index_of_destination);
  if (!output.IsConnectedToInput(input))
    return false;
  output.DisconnectInput(input);
  connected_nodes_[output_index]->erase(&destination);
  return true;
}

bool AudioNode::DisconnectFromOutputIfConnected(unsigned output_index,
                                                AudioParam& param) {
  AudioNodeOutput& output = Handler().Output(output_index);
  if (!output.IsConnectedToAudioParam(param.Handler()))
    return false;
  output.DisconnectAudioParam(param.Handler());
  connected_params_[output_index]->erase(&param);
  return true;
}

void AudioNode::disconnect() {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(context());

  // Disconnect all outgoing connections.
  for (unsigned i = 0; i < numberOfOutputs(); ++i)
    DisconnectAllFromOutput(i);

  Handler().UpdatePullStatusIfNeeded();
}

void AudioNode::disconnect(unsigned output_index,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(context());

  // Sanity check on the output index.
  if (output_index >= numberOfOutputs()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "output index", output_index, 0u,
            ExceptionMessages::kInclusiveBound, numberOfOutputs() - 1,
            ExceptionMessages::kInclusiveBound));
    return;
  }
  // Disconnect all outgoing connections from the given output.
  DisconnectAllFromOutput(output_index);

  Handler().UpdatePullStatusIfNeeded();
}

void AudioNode::disconnect(AudioNode* destination,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(context());

  unsigned number_of_disconnections = 0;

  // FIXME: Can this be optimized? ChannelSplitter and ChannelMerger can have
  // 32 ports and that requires 1024 iterations to validate entire connections.
  for (unsigned output_index = 0; output_index < numberOfOutputs();
       ++output_index) {
    for (unsigned input_index = 0;
         input_index < destination->Handler().NumberOfInputs(); ++input_index) {
      if (DisconnectFromOutputIfConnected(output_index, *destination,
                                          input_index))
        number_of_disconnections++;
    }
  }

  // If there is no connection to the destination, throw an exception.
  if (number_of_disconnections == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "the given destination is not connected.");
    return;
  }

  Handler().UpdatePullStatusIfNeeded();
}

void AudioNode::disconnect(AudioNode* destination,
                           unsigned output_index,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(context());

  if (output_index >= numberOfOutputs()) {
    // The output index is out of range. Throw an exception.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "output index", output_index, 0u,
            ExceptionMessages::kInclusiveBound, numberOfOutputs() - 1,
            ExceptionMessages::kInclusiveBound));
    return;
  }

  // If the output index is valid, proceed to disconnect.
  unsigned number_of_disconnections = 0;
  // Sanity check on destination inputs and disconnect when possible.
  for (unsigned input_index = 0; input_index < destination->numberOfInputs();
       ++input_index) {
    if (DisconnectFromOutputIfConnected(output_index, *destination,
                                        input_index))
      number_of_disconnections++;
  }

  // If there is no connection to the destination, throw an exception.
  if (number_of_disconnections == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "output (" + String::Number(output_index) +
            ") is not connected to the given destination.");
  }

  Handler().UpdatePullStatusIfNeeded();
}

void AudioNode::disconnect(AudioNode* destination,
                           unsigned output_index,
                           unsigned input_index,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(context());

  if (output_index >= numberOfOutputs()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "output index", output_index, 0u,
            ExceptionMessages::kInclusiveBound, numberOfOutputs() - 1,
            ExceptionMessages::kInclusiveBound));
    return;
  }

  if (input_index >= destination->Handler().NumberOfInputs()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "input index", input_index, 0u, ExceptionMessages::kInclusiveBound,
            destination->numberOfInputs() - 1,
            ExceptionMessages::kInclusiveBound));
    return;
  }

  // If both indices are valid, proceed to disconnect.
  if (!DisconnectFromOutputIfConnected(output_index, *destination,
                                       input_index)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "output (" + String::Number(output_index) +
            ") is not connected to the input (" + String::Number(input_index) +
            ") of the destination.");
    return;
  }

  Handler().UpdatePullStatusIfNeeded();
}

void AudioNode::disconnect(AudioParam* destination_param,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(context());

  // The number of disconnection made.
  unsigned number_of_disconnections = 0;

  // Check if the node output is connected the destination AudioParam.
  // Disconnect if connected and increase |numberOfDisconnectios| by 1.
  for (unsigned output_index = 0; output_index < Handler().NumberOfOutputs();
       ++output_index) {
    if (DisconnectFromOutputIfConnected(output_index, *destination_param))
      number_of_disconnections++;
  }

  // Throw an exception when there is no valid connection to the destination.
  if (number_of_disconnections == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "the given AudioParam is not connected.");
    return;
  }

  Handler().UpdatePullStatusIfNeeded();
}

void AudioNode::disconnect(AudioParam* destination_param,
                           unsigned output_index,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(context());

  if (output_index >= Handler().NumberOfOutputs()) {
    // The output index is out of range. Throw an exception.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "output index", output_index, 0u,
            ExceptionMessages::kInclusiveBound, numberOfOutputs() - 1,
            ExceptionMessages::kInclusiveBound));
    return;
  }

  // If the output index is valid, proceed to disconnect.
  if (!DisconnectFromOutputIfConnected(output_index, *destination_param)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "specified destination AudioParam and node output (" +
            String::Number(output_index) + ") are not connected.");
    return;
  }

  Handler().UpdatePullStatusIfNeeded();
}

unsigned AudioNode::numberOfInputs() const {
  return Handler().NumberOfInputs();
}

unsigned AudioNode::numberOfOutputs() const {
  return Handler().NumberOfOutputs();
}

unsigned long AudioNode::channelCount() const {
  return Handler().ChannelCount();
}

void AudioNode::setChannelCount(unsigned long count,
                                ExceptionState& exception_state) {
  Handler().SetChannelCount(count, exception_state);
}

String AudioNode::channelCountMode() const {
  return Handler().GetChannelCountMode();
}

void AudioNode::setChannelCountMode(const String& mode,
                                    ExceptionState& exception_state) {
  Handler().SetChannelCountMode(mode, exception_state);
}

String AudioNode::channelInterpretation() const {
  return Handler().ChannelInterpretation();
}

void AudioNode::setChannelInterpretation(const String& interpretation,
                                         ExceptionState& exception_state) {
  Handler().SetChannelInterpretation(interpretation, exception_state);
}

const AtomicString& AudioNode::InterfaceName() const {
  return EventTargetNames::AudioNode;
}

ExecutionContext* AudioNode::GetExecutionContext() const {
  return context()->GetExecutionContext();
}

void AudioNode::DidAddOutput(unsigned number_of_outputs) {
  connected_nodes_.push_back(nullptr);
  DCHECK_EQ(number_of_outputs, connected_nodes_.size());
  connected_params_.push_back(nullptr);
  DCHECK_EQ(number_of_outputs, connected_params_.size());
}

}  // namespace blink
