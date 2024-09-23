// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_node.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_param_descriptor.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_definition.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

AudioWorkletNode::AudioWorkletNode(
    BaseAudioContext& context,
    const String& name,
    const AudioWorkletNodeOptions* options,
    const Vector<CrossThreadAudioParamInfo> param_info_list,
    MessagePort* node_port)
    : AudioNode(context),
      ActiveScriptWrappable<AudioWorkletNode>({}),
      node_port_(node_port) {
  HeapHashMap<String, Member<AudioParam>> audio_param_map;
  HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map;
  for (const auto& param_info : param_info_list) {
    String param_name = param_info.Name();
    AudioParamHandler::AutomationRate param_automation_rate(
        AudioParamHandler::AutomationRate::kAudio);
    if (param_info.AutomationRate() == "k-rate") {
      param_automation_rate = AudioParamHandler::AutomationRate::kControl;
    }
    AudioParam* audio_param = AudioParam::Create(
        context, Uuid(), AudioParamHandler::kParamTypeAudioWorklet,
        param_info.DefaultValue(), param_automation_rate,
        AudioParamHandler::AutomationRateMode::kVariable, param_info.MinValue(),
        param_info.MaxValue());
    audio_param->SetCustomParamName("AudioWorkletNode(\"" + name + "\")." +
                                    param_name);
    audio_param_map.Set(param_name, audio_param);
    param_handler_map.Set(param_name, WrapRefCounted(&audio_param->Handler()));

    if (options->hasParameterData()) {
      for (const auto& key_value_pair : options->parameterData()) {
        if (key_value_pair.first == param_name) {
          audio_param->setValue(key_value_pair.second);
        }
      }
    }
  }
  parameter_map_ = MakeGarbageCollected<AudioParamMap>(audio_param_map);

  SetHandler(AudioWorkletHandler::Create(*this, context.sampleRate(), name,
                                         param_handler_map, options));
}

AudioWorkletNode* AudioWorkletNode::Create(
    ScriptState* script_state,
    BaseAudioContext* context,
    const String& name,
    const AudioWorkletNodeOptions* options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (options->numberOfInputs() == 0 && options->numberOfOutputs() == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "AudioWorkletNode cannot be created: Number of inputs and number of "
        "outputs cannot be both zero.");
    return nullptr;
  }

  if (options->hasOutputChannelCount()) {
    if (options->numberOfOutputs() != options->outputChannelCount().size()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kIndexSizeError,
          "AudioWorkletNode cannot be created: Length of specified "
          "'outputChannelCount' (" +
              String::Number(options->outputChannelCount().size()) +
              ") does not match the given number of outputs (" +
              String::Number(options->numberOfOutputs()) + ").");
      return nullptr;
    }

    for (const auto& channel_count : options->outputChannelCount()) {
      if (channel_count < 1 ||
          channel_count > BaseAudioContext::MaxNumberOfChannels()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotSupportedError,
            ExceptionMessages::IndexOutsideRange<uint32_t>(
                "channel count", channel_count, 1,
                ExceptionMessages::kInclusiveBound,
                BaseAudioContext::MaxNumberOfChannels(),
                ExceptionMessages::kInclusiveBound));
        return nullptr;
      }
    }
  }

  if (!context->audioWorklet()->IsReady()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "AudioWorkletNode cannot be created: AudioWorklet does not have a "
        "valid AudioWorkletGlobalScope. Load a script via "
        "audioWorklet.addModule() first.");
    return nullptr;
  }

  if (!context->audioWorklet()->IsProcessorRegistered(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "AudioWorkletNode cannot be created: The node name '" + name +
            "' is not defined in AudioWorkletGlobalScope.");
    return nullptr;
  }

  if (context->IsContextCleared()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "AudioWorkletNode cannot be created: No execution context available.");
    return nullptr;
  }

  auto* channel =
      MakeGarbageCollected<MessageChannel>(context->GetExecutionContext());
  MessagePortChannel processor_port_channel = channel->port2()->Disentangle();

  AudioWorkletNode* node = MakeGarbageCollected<AudioWorkletNode>(
      *context, name, options,
      context->audioWorklet()->GetParamInfoListForProcessor(name),
      channel->port1());

  if (!node) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "AudioWorkletNode cannot be created.");
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  // context keeps reference as a source node if the node has a valid output.
  // The node with zero output cannot be a source, so it won't be added as an
  // active source node.
  if (node->numberOfOutputs() > 0) {
    context->NotifySourceNodeStartedProcessing(node);
  }

  v8::Isolate* isolate = script_state->GetIsolate();
  SerializedScriptValue::SerializeOptions serialize_options;
  serialize_options.for_storage = SerializedScriptValue::kNotForStorage;

  // The node options must be serialized since they are passed to and consumed
  // by a worklet thread.
  scoped_refptr<SerializedScriptValue> serialized_node_options =
      SerializedScriptValue::Serialize(
          isolate,
          ToV8Traits<AudioWorkletNodeOptions>::ToV8(script_state, options),
          serialize_options, exception_state);

  // `serialized_node_options` can be nullptr if the option dictionary is not
  // valid.
  if (!serialized_node_options) {
    serialized_node_options = SerializedScriptValue::NullValue();
  }
  DCHECK(serialized_node_options);

  // This is non-blocking async call. `node` still can be returned to user
  // before the scheduled async task is completed.
  context->audioWorklet()->CreateProcessor(node->GetWorkletHandler(),
                                           std::move(processor_port_channel),
                                           std::move(serialized_node_options));

  {
    // The node should be manually added to the automatic pull node list,
    // even without a `connect()` call.
    DeferredTaskHandler::GraphAutoLocker locker(context);
    node->Handler().UpdatePullStatusIfNeeded();
  }

  return node;
}

bool AudioWorkletNode::HasPendingActivity() const {
  return GetWorkletHandler()->IsProcessorActive();
}

AudioParamMap* AudioWorkletNode::parameters() const {
  return parameter_map_.Get();
}

MessagePort* AudioWorkletNode::port() const {
  return node_port_.Get();
}

void AudioWorkletNode::FireProcessorError(
    AudioWorkletProcessorErrorState error_state) {
  DCHECK(IsMainThread());
  DCHECK(error_state == AudioWorkletProcessorErrorState::kConstructionError ||
         error_state == AudioWorkletProcessorErrorState::kProcessError);

  String error_message = "an error thrown from ";
  switch (error_state) {
    case AudioWorkletProcessorErrorState::kNoError:
      NOTREACHED_IN_MIGRATION();
      return;
    case AudioWorkletProcessorErrorState::kConstructionError:
      error_message = error_message + "AudioWorkletProcessor constructor";
      break;
    case AudioWorkletProcessorErrorState::kProcessError:
      error_message = error_message + "AudioWorkletProcessor::process() method";
      break;
  }
  ErrorEvent* event = ErrorEvent::Create(
      error_message, CaptureSourceLocation(GetExecutionContext()), nullptr);
  DispatchEvent(*event);
}

scoped_refptr<AudioWorkletHandler> AudioWorkletNode::GetWorkletHandler() const {
  return WrapRefCounted(&static_cast<AudioWorkletHandler&>(Handler()));
}

void AudioWorkletNode::Trace(Visitor* visitor) const {
  visitor->Trace(parameter_map_);
  visitor->Trace(node_port_);
  AudioNode::Trace(visitor);
}

void AudioWorkletNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  for (const auto& param_name : parameter_map_->GetHashMap().Keys()) {
    GraphTracer().DidCreateAudioParam(
        parameter_map_->GetHashMap().at(param_name));
  }
}

void AudioWorkletNode::ReportWillBeDestroyed() {
  for (const auto& param_name : parameter_map_->GetHashMap().Keys()) {
    GraphTracer().WillDestroyAudioParam(
        parameter_map_->GetHashMap().at(param_name));
  }
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
