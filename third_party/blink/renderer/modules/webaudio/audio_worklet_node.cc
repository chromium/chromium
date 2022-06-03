// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_node.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_param_descriptor.h"
#include "third_party/blink/renderer/core/events/error_event.h"
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
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

AudioWorkletHandler::AudioWorkletHandler(
    AudioNode& node,
    float sample_rate,
    String name,
    HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map,
    const AudioWorkletNodeOptions* options)
    : AudioHandler(kNodeTypeAudioWorklet, node, sample_rate),
      name_(name),
      param_handler_map_(param_handler_map) {
  DCHECK(IsMainThread());

  for (const auto& param_name : param_handler_map_.Keys()) {
    param_value_map_.Set(param_name,
                         std::make_unique<AudioFloatArray>(
                             GetDeferredTaskHandler().RenderQuantumFrames()));
  }

  for (unsigned i = 0; i < options->numberOfInputs(); ++i)
    AddInput();
  // The number of inputs does not change after the construnction, so it is
  // safe to reserve the array capacity and size.
  inputs_.ReserveInitialCapacity(options->numberOfInputs());
  inputs_.resize(options->numberOfInputs());

  is_output_channel_count_given_ = options->hasOutputChannelCount();

  for (unsigned i = 0; i < options->numberOfOutputs(); ++i) {
    // If |options->outputChannelCount| unspecified, all outputs are mono.
    AddOutput(is_output_channel_count_given_ ? options->outputChannelCount()[i]
                                             : 1);
  }
  // Same for the outputs as well.
  outputs_.ReserveInitialCapacity(options->numberOfOutputs());
  outputs_.resize(options->numberOfOutputs());

  if (Context()->GetExecutionContext()) {
    // Cross-thread tasks between AWN/AWP is okay to be throttled, thus
    // kMiscPlatformAPI. It is for post-creation/destruction chores.
    main_thread_task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
        TaskType::kMiscPlatformAPI);
    DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  }

  Initialize();
}

AudioWorkletHandler::~AudioWorkletHandler() {
  inputs_.clear();
  outputs_.clear();
  param_handler_map_.clear();
  param_value_map_.clear();
  Uninitialize();
}

scoped_refptr<AudioWorkletHandler> AudioWorkletHandler::Create(
    AudioNode& node,
    float sample_rate,
    String name,
    HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map,
    const AudioWorkletNodeOptions* options) {
  return base::AdoptRef(new AudioWorkletHandler(node, sample_rate, name,
                                                param_handler_map, options));
}

void AudioWorkletHandler::Process(uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "AudioWorkletHandler::Process");

  // Render and update the node state when the processor is ready with no error.
  // We also need to check if the global scope is valid before we request
  // the rendering in the AudioWorkletGlobalScope.
  if (processor_ && !processor_->hasErrorOccurred()) {
    // If the input is not connected, inform the processor with nullptr.
    for (unsigned i = 0; i < NumberOfInputs(); ++i)
      inputs_[i] = Input(i).IsConnected() ? Input(i).Bus() : nullptr;
    for (unsigned i = 0; i < NumberOfOutputs(); ++i)
      outputs_[i] = WrapRefCounted(Output(i).Bus());

    for (const auto& param_name : param_value_map_.Keys()) {
      auto* const param_handler = param_handler_map_.at(param_name);
      AudioFloatArray* param_values = param_value_map_.at(param_name);
      if (param_handler->HasSampleAccurateValues() &&
          param_handler->IsAudioRate()) {
        param_handler->CalculateSampleAccurateValues(
            param_values->Data(), static_cast<uint32_t>(frames_to_process));
      } else {
        std::fill(param_values->Data(),
                  param_values->Data() + frames_to_process,
                  param_handler->FinalValue());
      }
    }

    // Run the render code and check the state of processor. Finish the
    // processor if needed.
    if (!processor_->Process(inputs_, outputs_, param_value_map_) ||
        processor_->hasErrorOccurred()) {
      FinishProcessorOnRenderThread();
    }
  } else {
    // The initialization of handler or the associated processor might not be
    // ready yet or it is in the error state. If so, zero out the connected
    // output.
    for (unsigned i = 0; i < NumberOfOutputs(); ++i)
      Output(i).Bus()->Zero();
  }
}

void AudioWorkletHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();
  DCHECK(input);

  // Dynamic channel count only works when the node has 1 input, 1 output and
  // |outputChannelCount| is not given. Otherwise the channel count(s) should
  // not be dynamically changed.
  if (NumberOfInputs() == 1 && NumberOfOutputs() == 1 &&
      !is_output_channel_count_given_) {
    DCHECK_EQ(input, &Input(0));
    unsigned number_of_input_channels = Input(0).NumberOfChannels();
    if (number_of_input_channels != Output(0).NumberOfChannels()) {
      // This will propagate the channel count to any nodes connected further
      // downstream in the graph.
      Output(0).SetNumberOfChannels(number_of_input_channels);
    }
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
  UpdatePullStatusIfNeeded();
}

void AudioWorkletHandler::UpdatePullStatusIfNeeded() {
  Context()->AssertGraphOwner();

  bool is_output_connected = false;
  for (unsigned i = 0; i < NumberOfOutputs(); ++i) {
    if (Output(i).IsConnected()) {
      is_output_connected = true;
      break;
    }
  }

  // If no output is connected, add the node to the automatic pull list.
  // Otherwise, remove it out of the list.
  if (!is_output_connected) {
    Context()->GetDeferredTaskHandler().AddAutomaticPullNode(this);
  } else {
    Context()->GetDeferredTaskHandler().RemoveAutomaticPullNode(this);
  }
}

double AudioWorkletHandler::TailTime() const {
  DCHECK(Context()->IsAudioThread());
  return tail_time_;
}

void AudioWorkletHandler::SetProcessorOnRenderThread(
    AudioWorkletProcessor* processor) {
  // TODO(hongchan): unify the thread ID check. The thread ID for this call
  // is different from |Context()->IsAudiothread()|.
  DCHECK(!IsMainThread());

  // |processor| can be nullptr when the invocation of user-supplied constructor
  // fails. That failure fires at the node's 'onprocessorerror' event handler.
  if (processor) {
    processor_ = processor;
  } else {
    PostCrossThreadTask(
        *main_thread_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletHandler::NotifyProcessorError, AsWeakPtr(),
            AudioWorkletProcessorErrorState::kConstructionError));
  }
}

void AudioWorkletHandler::FinishProcessorOnRenderThread() {
  DCHECK(Context()->IsAudioThread());

  // If the user-supplied code is not runnable (i.e. threw an exception)
  // anymore after the process() call above. Invoke error on the main thread.
  AudioWorkletProcessorErrorState error_state = processor_->GetErrorState();
  if (error_state == AudioWorkletProcessorErrorState::kProcessError) {
    PostCrossThreadTask(
        *main_thread_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&AudioWorkletHandler::NotifyProcessorError,
                            AsWeakPtr(), error_state));
  }

  // TODO(hongchan): After this point, The handler has no more pending activity
  // and ready for GC.
  Context()->NotifySourceNodeFinishedProcessing(this);
  processor_.Clear();
  tail_time_ = 0;
}

void AudioWorkletHandler::NotifyProcessorError(
    AudioWorkletProcessorErrorState error_state) {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext() || !GetNode())
    return;

  static_cast<AudioWorkletNode*>(GetNode())->FireProcessorError(error_state);
}

// ----------------------------------------------------------------

AudioWorkletNode::AudioWorkletNode(
    BaseAudioContext& context,
    const String& name,
    const AudioWorkletNodeOptions* options,
    const Vector<CrossThreadAudioParamInfo> param_info_list,
    MessagePort* node_port)
    : AudioNode(context), node_port_(node_port) {
  HeapHashMap<String, Member<AudioParam>> audio_param_map;
  HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map;
  for (const auto& param_info : param_info_list) {
    String param_name = param_info.Name().IsolatedCopy();
    AudioParamHandler::AutomationRate param_automation_rate(
        AudioParamHandler::AutomationRate::kAudio);
    if (param_info.AutomationRate() == "k-rate")
      param_automation_rate = AudioParamHandler::AutomationRate::kControl;
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
        if (key_value_pair.first == param_name)
          audio_param->setValue(key_value_pair.second);
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
          ToV8Traits<AudioWorkletNodeOptions>::ToV8(script_state, options)
              .ToLocalChecked(),
          serialize_options, exception_state);

  // |serialized_node_options| can be nullptr if the option dictionary is not
  // valid.
  if (!serialized_node_options) {
    serialized_node_options = SerializedScriptValue::NullValue();
  }
  DCHECK(serialized_node_options);

  // This is non-blocking async call. |node| still can be returned to user
  // before the scheduled async task is completed.
  context->audioWorklet()->CreateProcessor(node->GetWorkletHandler(),
                                           std::move(processor_port_channel),
                                           std::move(serialized_node_options));

  {
    // The node should be manually added to the automatic pull node list,
    // even without a |connect()| call.
    BaseAudioContext::GraphAutoLocker locker(context);
    node->Handler().UpdatePullStatusIfNeeded();
  }

  return node;
}

bool AudioWorkletNode::HasPendingActivity() const {
  return !context()->IsContextCleared();
}

AudioParamMap* AudioWorkletNode::parameters() const {
  return parameter_map_;
}

MessagePort* AudioWorkletNode::port() const {
  return node_port_;
}

void AudioWorkletNode::FireProcessorError(
    AudioWorkletProcessorErrorState error_state) {
  DCHECK(IsMainThread());
  DCHECK(error_state == AudioWorkletProcessorErrorState::kConstructionError ||
         error_state == AudioWorkletProcessorErrorState::kProcessError);

  String error_message = "an error thrown from ";
  switch (error_state) {
    case AudioWorkletProcessorErrorState::kNoError:
      NOTREACHED();
      return;
    case AudioWorkletProcessorErrorState::kConstructionError:
      error_message = error_message + "AudioWorkletProcessor constructor";
      break;
    case AudioWorkletProcessorErrorState::kProcessError:
      error_message = error_message + "AudioWorkletProcessor::process() method";
      break;
  }
  ErrorEvent* event = ErrorEvent::Create(
      error_message, SourceLocation::Capture(GetExecutionContext()), nullptr);
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
