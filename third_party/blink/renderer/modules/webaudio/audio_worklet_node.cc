// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_node.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param_descriptor.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_definition.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
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
                             audio_utilities::kRenderQuantumFrames));
  }

  for (unsigned i = 0; i < options->numberOfInputs(); ++i) {
    AddInput();
  }

  if (options->hasOutputChannelCount()) {
    is_output_channel_count_given_ = true;
  }

  for (unsigned i = 0; i < options->numberOfOutputs(); ++i) {
    // If |options->outputChannelCount| unspecified, all outputs are mono.
    AddOutput(is_output_channel_count_given_ ? options->outputChannelCount()[i]
                                             : 1);
  }

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

  // Render and update the node state when the processor is ready with no error.
  // We also need to check if the global scope is valid before we request
  // the rendering in the AudioWorkletGlobalScope.
  if (processor_ && !processor_->hasErrorOccured()) {
    Vector<AudioBus*> input_buses;
    Vector<AudioBus*> output_buses;
    for (unsigned i = 0; i < NumberOfInputs(); ++i) {
      // If the input is not connected, inform the processor of that
      // fact by setting the bus to null.
      AudioBus* bus = Input(i).IsConnected() ? Input(i).Bus() : nullptr;
      input_buses.push_back(bus);
    }
    for (unsigned i = 0; i < NumberOfOutputs(); ++i)
      output_buses.push_back(Output(i).Bus());

    for (const auto& param_name : param_value_map_.Keys()) {
      auto* const param_handler = param_handler_map_.at(param_name);
      AudioFloatArray* param_values = param_value_map_.at(param_name);
      if (param_handler->HasSampleAccurateValues()) {
        param_handler->CalculateSampleAccurateValues(
            param_values->Data(), static_cast<uint32_t>(frames_to_process));
      } else {
        std::fill(param_values->Data(),
                  param_values->Data() + frames_to_process,
                  param_handler->Value());
      }
    }

    // Run the render code and check the state of processor. Finish the
    // processor if needed.
    if (!processor_->Process(&input_buses, &output_buses, &param_value_map_) ||
        processor_->hasErrorOccured()) {
      FinishProcessorOnRenderThread();
    }
  } else {
    // The initialization of handler or the associated processor might not be
    // ready yet or it is in the error state. If so, zero out the connected
    // output.
    for (unsigned i = 0; i < NumberOfOutputs(); ++i) {
      Output(i).Bus()->Zero();
    }
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
    DCHECK_EQ(input, &this->Input(0));
    unsigned number_of_input_channels = Input(0).NumberOfChannels();
    if (number_of_input_channels != Output(0).NumberOfChannels()) {
      // This will propagate the channel count to any nodes connected further
      // downstream in the graph.
      Output(0).SetNumberOfChannels(number_of_input_channels);
    }
  }

  // If the node has zero output, it becomes the "automatic pull" node. This
  // does not apply to the general case where we have outputs that aren't
  // connected.
  if (NumberOfOutputs() == 0) {
    Context()->GetDeferredTaskHandler().AddAutomaticPullNode(this);
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
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
            &AudioWorkletHandler::NotifyProcessorError, WrapRefCounted(this),
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
                            WrapRefCounted(this), error_state));
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

  static_cast<AudioWorkletNode*>(GetNode())->FireProcessorError();
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
    AudioParam* audio_param = AudioParam::Create(
        context, Uuid(), AudioParamHandler::kParamTypeAudioWorklet,
        param_info.DefaultValue(), AudioParamHandler::AutomationRate::kAudio,
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

  SetHandler(AudioWorkletHandler::Create(*this,
                                         context.sampleRate(),
                                         name,
                                         param_handler_map,
                                         options));
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
          ToV8(options, script_state->GetContext()->Global(), isolate),
          serialize_options,
          exception_state);

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

  return node;
}

bool AudioWorkletNode::HasPendingActivity() const {
  return !context()->IsContextClosed();
}

AudioParamMap* AudioWorkletNode::parameters() const {
  return parameter_map_;
}

MessagePort* AudioWorkletNode::port() const {
  return node_port_;
}

void AudioWorkletNode::FireProcessorError() {
  DispatchEvent(*Event::Create(event_type_names::kProcessorerror));
}

scoped_refptr<AudioWorkletHandler> AudioWorkletNode::GetWorkletHandler() const {
  return WrapRefCounted(&static_cast<AudioWorkletHandler&>(Handler()));
}

void AudioWorkletNode::Trace(blink::Visitor* visitor) {
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
