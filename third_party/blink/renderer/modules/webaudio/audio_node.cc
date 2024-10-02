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

#include <inttypes.h>

#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_node_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_channel_count_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_channel_interpretation.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_handler.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_wiring.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

AudioNode::AudioNode(BaseAudioContext& context)
    : InspectorHelperMixin(context.GraphTracer(), context.Uuid()),
      context_(context),
      deferred_task_handler_(&context.GetDeferredTaskHandler()),
      handler_(nullptr) {}

AudioNode::~AudioNode() {
  // The graph lock is required to destroy the handler. And we can't use
  // `context_` to touch it, since that object may also be a dead heap object.
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
  DeferredTaskHandler::GraphAutoLocker locker(context());
  Handler().Dispose();

  // Add the handler to the orphan list.  This keeps the handler alive until it
  // can be deleted at a safe point (in pre/post handler task).  If the graph is
  // being processed, the handler must be added.  If the context is suspended,
  // the handler still needs to be added in case the context is resumed.
  DCHECK(context());
  if (context()->IsPullingAudioGraph() ||
      context()->ContextState() == BaseAudioContext::kSuspended) {
    context()->GetDeferredTaskHandler().AddRenderingOrphanHandler(
        std::move(handler_));
  }

  // Notify the inspector that this node is going away. The actual clean up
  // will be done in the subclass implementation.
  ReportWillBeDestroyed();
}

void AudioNode::SetHandler(scoped_refptr<AudioHandler> handler) {
  DCHECK(handler);
  handler_ = std::move(handler);

  // Unless the node is an AudioDestinationNode, notify the inspector that the
  // construction is completed. The actual report will be done in the subclass
  // implementation. (A destination node is owned by the context and will be
  // reported by it.)
  if (handler_->GetNodeType() != AudioHandler::NodeType::kNodeTypeDestination) {
    ReportDidCreate();
  }

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: %16p: %2d: AudioNode::AudioNode %16p\n", context(),
          this, handler_->GetNodeType(), handler_.get());
#endif
}

bool AudioNode::ContainsHandler() const {
  return handler_.get();
}

AudioHandler& AudioNode::Handler() const {
  return *handler_;
}

void AudioNode::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  visitor->Trace(connected_nodes_);
  visitor->Trace(connected_params_);
  InspectorHelperMixin::Trace(visitor);
  EventTarget::Trace(visitor);
}

void AudioNode::HandleChannelOptions(const AudioNodeOptions* options,
                                     ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (options->hasChannelCount()) {
    setChannelCount(options->channelCount(), exception_state);
  }
  if (options->hasChannelCountMode()) {
    setChannelCountMode(options->channelCountMode(), exception_state);
  }
  if (options->hasChannelInterpretation()) {
    setChannelInterpretation(options->channelInterpretation(), exception_state);
  }
}

String AudioNode::GetNodeName() const {
  return Handler().NodeTypeName();
}

BaseAudioContext* AudioNode::context() const {
  return context_.Get();
}

AudioNode* AudioNode::connect(AudioNode* destination,
                              unsigned output_index,
                              unsigned input_index,
                              ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(context());

  context()->WarnForConnectionIfContextClosed();

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
        "cannot connect to an AudioNode "
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

  SendLogMessage(
      __func__, String::Format(
                    "({output=[index:%u, type:%s, handler:0x%" PRIXPTR "]} --> "
                    "{input=[index:%u, type:%s, handler:0x%" PRIXPTR "]})",
                    output_index, Handler().NodeTypeName().Utf8().c_str(),
                    reinterpret_cast<uintptr_t>(&Handler()), input_index,
                    destination->Handler().NodeTypeName().Utf8().c_str(),
                    reinterpret_cast<uintptr_t>(&destination->Handler())));

  AudioNodeWiring::Connect(Handler().Output(output_index),
                           destination->Handler().Input(input_index));
  if (!connected_nodes_[output_index]) {
    connected_nodes_[output_index] =
        MakeGarbageCollected<HeapHashSet<Member<AudioNode>>>();
  }
  connected_nodes_[output_index]->insert(destination);

  Handler().UpdatePullStatusIfNeeded();

  GraphTracer().DidConnectNodes(this, destination, output_index, input_index);

  return destination;
}

void AudioNode::connect(AudioParam* param,
                        unsigned output_index,
                        ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(context());

  context()->WarnForConnectionIfContextClosed();

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
        DOMExceptionCode::kInvalidAccessError,
        "cannot connect to an AudioParam "
        "belonging to a different audio context.");
    return;
  }

  AudioNodeWiring::Connect(Handler().Output(output_index), param->Handler());
  if (!connected_params_[output_index]) {
    connected_params_[output_index] =
        MakeGarbageCollected<HeapHashSet<Member<AudioParam>>>();
  }
  connected_params_[output_index]->insert(param);

  Handler().UpdatePullStatusIfNeeded();

  GraphTracer().DidConnectNodeParam(this, param, output_index);
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
  if (!AudioNodeWiring::IsConnected(output, input)) {
    return false;
  }
  AudioNodeWiring::Disconnect(output, input);
  connected_nodes_[output_index]->erase(&destination);
  return true;
}

bool AudioNode::DisconnectFromOutputIfConnected(unsigned output_index,
                                                AudioParam& param) {
  AudioNodeOutput& output = Handler().Output(output_index);
  if (!AudioNodeWiring::IsConnected(output, param.Handler())) {
    return false;
  }
  AudioNodeWiring::Disconnect(output, param.Handler());
  connected_params_[output_index]->erase(&param);
  return true;
}

void AudioNode::disconnect() {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(context());

  // Disconnect all outgoing connections.
  for (unsigned i = 0; i < numberOfOutputs(); ++i) {
    DisconnectAllFromOutput(i);
  }

  Handler().UpdatePullStatusIfNeeded();

  GraphTracer().DidDisconnectNodes(this);
}

void AudioNode::disconnect(unsigned output_index,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(context());

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

  GraphTracer().DidDisconnectNodes(this, nullptr, output_index);
}

void AudioNode::disconnect(AudioNode* destination,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context() != destination->context()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "cannot disconnect from an AudioNode "
        "belonging to a different audio context.");
    return;
  }

  DeferredTaskHandler::GraphAutoLocker locker(context());

  unsigned number_of_disconnections = 0;

  // FIXME: Can this be optimized? ChannelSplitter and ChannelMerger can have
  // 32 ports and that requires 1024 iterations to validate entire connections.
  for (unsigned output_index = 0; output_index < numberOfOutputs();
       ++output_index) {
    for (unsigned input_index = 0;
         input_index < destination->Handler().NumberOfInputs(); ++input_index) {
      if (DisconnectFromOutputIfConnected(output_index, *destination,
                                          input_index)) {
        number_of_disconnections++;
      }
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

  GraphTracer().DidDisconnectNodes(this, destination);
}

void AudioNode::disconnect(AudioNode* destination,
                           unsigned output_index,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context() != destination->context()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "cannot disconnect from an AudioNode "
        "belonging to a different audio context.");
    return;
  }

  DeferredTaskHandler::GraphAutoLocker locker(context());

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
                                        input_index)) {
      number_of_disconnections++;
    }
  }

  // If there is no connection to the destination, throw an exception.
  if (number_of_disconnections == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "output (" + String::Number(output_index) +
            ") is not connected to the given destination.");
  }

  Handler().UpdatePullStatusIfNeeded();

  GraphTracer().DidDisconnectNodes(this, destination, output_index);
}

void AudioNode::disconnect(AudioNode* destination,
                           unsigned output_index,
                           unsigned input_index,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context() != destination->context()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "cannot disconnect from an AudioNode "
        "belonging to a different audio context.");
    return;
  }

  DeferredTaskHandler::GraphAutoLocker locker(context());

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

  GraphTracer().DidDisconnectNodes(
      this, destination, output_index, input_index);
}

void AudioNode::disconnect(AudioParam* destination_param,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context() != destination_param->Context()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "cannot disconnect from an AudioParam "
        "belonging to a different audio context.");
    return;
  }

  DeferredTaskHandler::GraphAutoLocker locker(context());

  // The number of disconnection made.
  unsigned number_of_disconnections = 0;

  // Check if the node output is connected the destination AudioParam.
  // Disconnect if connected and increase `number_of_disconnections` by 1.
  for (unsigned output_index = 0; output_index < Handler().NumberOfOutputs();
       ++output_index) {
    if (DisconnectFromOutputIfConnected(output_index, *destination_param)) {
      number_of_disconnections++;
    }
  }

  // Throw an exception when there is no valid connection to the destination.
  if (number_of_disconnections == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "the given AudioParam is not connected.");
    return;
  }

  Handler().UpdatePullStatusIfNeeded();

  GraphTracer().DidDisconnectNodeParam(this, destination_param);
}

void AudioNode::disconnect(AudioParam* destination_param,
                           unsigned output_index,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(context());

  if (context() != destination_param->Context()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "cannot disconnect from an AudioParam belonging to a different "
        "BaseAudioContext.");
    return;
  }

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

  GraphTracer().DidDisconnectNodeParam(this, destination_param, output_index);
}

unsigned AudioNode::numberOfInputs() const {
  return Handler().NumberOfInputs();
}

unsigned AudioNode::numberOfOutputs() const {
  return Handler().NumberOfOutputs();
}

unsigned AudioNode::channelCount() const {
  return Handler().ChannelCount();
}

void AudioNode::setChannelCount(unsigned count,
                                ExceptionState& exception_state) {
  Handler().SetChannelCount(count, exception_state);
}

V8ChannelCountMode AudioNode::channelCountMode() const {
  return V8ChannelCountMode(Handler().GetChannelCountMode());
}

void AudioNode::setChannelCountMode(const V8ChannelCountMode& mode,
                                    ExceptionState& exception_state) {
  Handler().SetChannelCountMode(mode.AsEnum(), exception_state);
}

V8ChannelInterpretation AudioNode::channelInterpretation() const {
  return V8ChannelInterpretation(Handler().ChannelInterpretation());
}

void AudioNode::setChannelInterpretation(
    const V8ChannelInterpretation& interpretation,
    ExceptionState& exception_state) {
  Handler().SetChannelInterpretation(interpretation.AsEnum(), exception_state);
}

const AtomicString& AudioNode::InterfaceName() const {
  return event_target_names::kAudioNode;
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

void AudioNode::SendLogMessage(const char* const function_name,
                               const String& message) {
  WebRtcLogMessage(
      String::Format("[WA]AN::%s %s", function_name, message.Utf8().c_str())
          .Utf8());
}

}  // namespace blink
