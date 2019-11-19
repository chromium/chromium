// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_param_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_worklet_processor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_blink_audio_worklet_process_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_blink_audio_worklet_processor_constructor.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param_descriptor.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_definition.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_error_state.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"

namespace blink {

AudioWorkletGlobalScope::AudioWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread)
    : WorkletGlobalScope(std::move(creation_params),
                         thread->GetWorkerReportingProxy(),
                         thread) {}

AudioWorkletGlobalScope::~AudioWorkletGlobalScope() = default;

void AudioWorkletGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  is_closing_ = true;
  WorkletGlobalScope::Dispose();
}

void AudioWorkletGlobalScope::registerProcessor(
    const String& name,
    V8BlinkAudioWorkletProcessorConstructor* processor_ctor,
    ExceptionState& exception_state) {
  DCHECK(IsContextThread());

  if (processor_definition_map_.Contains(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  // TODO(hongchan): this is not stated in the spec, but seems necessary.
  // https://github.com/WebAudio/web-audio-api/issues/1172
  if (name.IsEmpty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  if (!processor_ctor->IsConstructor()) {
    exception_state.ThrowTypeError(
        "The provided callback is not a constructor.");
    return;
  }

  CallbackMethodRetriever retriever(processor_ctor);
  retriever.GetPrototypeObject(exception_state);
  if (exception_state.HadException())
    return;

  v8::Local<v8::Function> v8_process =
      retriever.GetMethodOrThrow("process", exception_state);
  if (exception_state.HadException())
    return;
  V8BlinkAudioWorkletProcessCallback* process =
      V8BlinkAudioWorkletProcessCallback::Create(v8_process);

  // constructor() and process() functions are successfully parsed from the
  // script code, thus create the definition. The rest of parsing process
  // (i.e. parameterDescriptors) is optional.
  AudioWorkletProcessorDefinition* definition =
      AudioWorkletProcessorDefinition::Create(name, processor_ctor, process);

  v8::Isolate* isolate = processor_ctor->GetIsolate();
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();

  v8::Local<v8::Value> v8_parameter_descriptors;
  {
    v8::TryCatch try_catch(isolate);
    if (!processor_ctor->CallbackObject()
             ->Get(current_context,
                   V8AtomicString(isolate, "parameterDescriptors"))
             .ToLocal(&v8_parameter_descriptors)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return;
    }
  }

  if (!v8_parameter_descriptors->IsNullOrUndefined()) {
    // Optional 'parameterDescriptors' property is found.
    const HeapVector<Member<AudioParamDescriptor>>& audio_param_descriptors =
        NativeValueTraits<IDLSequence<AudioParamDescriptor>>::NativeValue(
            isolate, v8_parameter_descriptors, exception_state);
    if (exception_state.HadException())
      return;
    definition->SetAudioParamDescriptors(audio_param_descriptors);
  }

  processor_definition_map_.Set(name, definition);
}

AudioWorkletProcessor* AudioWorkletGlobalScope::CreateProcessor(
    const String& name,
    MessagePortChannel message_port_channel,
    scoped_refptr<SerializedScriptValue> node_options) {
  DCHECK(IsContextThread());

  // The registered definition is already checked by AudioWorkletNode
  // construction process, so the |definition| here must be valid.
  AudioWorkletProcessorDefinition* definition = FindDefinition(name);
  DCHECK(definition);

  ScriptState* script_state = ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);

  // V8 object instance construction: this construction process is here to make
  // the AudioWorkletProcessor class a thin wrapper of v8::Object instance.
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);  // Route errors/exceptions to the dev console.

  DCHECK(!processor_creation_params_);
  // There is no way to pass additional constructor arguments that are not
  // described in Web IDL, the static constructor will look up
  // |processor_creation_params_| in the global scope to perform the
  // construction properly.
  base::AutoReset<std::unique_ptr<ProcessorCreationParams>>
      processor_creation_extra_param(
          &processor_creation_params_,
          std::make_unique<ProcessorCreationParams>(
              name, std::move(message_port_channel)));

  ScriptValue options(isolate,
                      ToV8(node_options->Deserialize(isolate), script_state));

  ScriptValue instance;
  if (!definition->ConstructorFunction()->Construct(options).To(&instance)) {
    return nullptr;
  }

  // ToImplWithTypeCheck() may return nullptr when the type does not match.
  AudioWorkletProcessor* processor =
      V8AudioWorkletProcessor::ToImplWithTypeCheck(isolate, instance.V8Value());

  if (processor) {
    processor_instances_.push_back(processor);
  }

  return processor;
}

bool AudioWorkletGlobalScope::Process(
    AudioWorkletProcessor* processor,
    Vector<AudioBus*>* input_buses,
    Vector<AudioBus*>* output_buses,
    HashMap<String, std::unique_ptr<AudioFloatArray>>* param_value_map) {
  CHECK_GE(input_buses->size(), 0u);
  CHECK_GE(output_buses->size(), 0u);

  ScriptState* script_state = ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> current_context = script_state->GetContext();
  AudioWorkletProcessorDefinition* definition =
      FindDefinition(processor->Name());
  DCHECK(definition);

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  // Prepare arguments of JS callback (inputs, outputs and param_values) with
  // directly using V8 API because the overhead of
  // ToV8(HeapVector<HeapVector<DOMFloat32Array>>) is not negligible and there
  // is no need to externalize the array buffers.

  // 1st arg of JS callback: inputs
  v8::Local<v8::Array> inputs = v8::Array::New(isolate, input_buses->size());
  uint32_t input_bus_index = 0;
  for (auto* const input_bus : *input_buses) {
    // If |input_bus| is null, then the input is not connected, and
    // the array for that input should have one channel and a length
    // of 0.
    unsigned number_of_channels = input_bus ? input_bus->NumberOfChannels() : 1;
    size_t bus_length = input_bus ? input_bus->length() : 0;

    v8::Local<v8::Array> channels = v8::Array::New(isolate, number_of_channels);
    bool success;
    if (!inputs
             ->CreateDataProperty(current_context, input_bus_index++, channels)
             .To(&success)) {
      return false;
    }
    for (uint32_t channel_index = 0; channel_index < number_of_channels;
         ++channel_index) {
      v8::Local<v8::ArrayBuffer> array_buffer =
          v8::ArrayBuffer::New(isolate, bus_length * sizeof(float));
      v8::Local<v8::Float32Array> float32_array =
          v8::Float32Array::New(array_buffer, 0, bus_length);
      if (!channels
               ->CreateDataProperty(current_context, channel_index,
                                    float32_array)
               .To(&success)) {
        return false;
      }
      const v8::ArrayBuffer::Contents& contents = array_buffer->GetContents();
      if (input_bus) {
        memcpy(contents.Data(), input_bus->Channel(channel_index)->Data(),
               bus_length * sizeof(float));
      }
    }
  }

  // 2nd arg of JS callback: outputs
  v8::Local<v8::Array> outputs = v8::Array::New(isolate, output_buses->size());
  uint32_t output_bus_counter = 0;

  // |output_array_buffers| stores underlying array buffers so that we can copy
  // them back to |output_buses|.
  Vector<Vector<v8::Local<v8::ArrayBuffer>>> output_array_buffers;
  output_array_buffers.ReserveInitialCapacity(output_buses->size());

  for (auto* const output_bus : *output_buses) {
    output_array_buffers.UncheckedAppend(Vector<v8::Local<v8::ArrayBuffer>>());
    output_array_buffers.back().ReserveInitialCapacity(
        output_bus->NumberOfChannels());
    v8::Local<v8::Array> channels =
        v8::Array::New(isolate, output_bus->NumberOfChannels());
    bool success;
    if (!outputs
             ->CreateDataProperty(current_context, output_bus_counter++,
                                  channels)
             .To(&success)) {
      return false;
    }
    for (uint32_t channel_index = 0;
         channel_index < output_bus->NumberOfChannels(); ++channel_index) {
      v8::Local<v8::ArrayBuffer> array_buffer =
          v8::ArrayBuffer::New(isolate, output_bus->length() * sizeof(float));
      v8::Local<v8::Float32Array> float32_array =
          v8::Float32Array::New(array_buffer, 0, output_bus->length());
      if (!channels
               ->CreateDataProperty(current_context, channel_index,
                                    float32_array)
               .To(&success)) {
        return false;
      }
      output_array_buffers.back().UncheckedAppend(array_buffer);
    }
  }

  // 3rd arg of JS callback: param_values
  v8::Local<v8::Object> param_values = v8::Object::New(isolate);
  for (const auto& param : *param_value_map) {
    const String& param_name = param.key;
    const AudioFloatArray* param_array = param.value.get();

    // If the AudioParam is constant, then the param array should have length 1.
    // Manually check to see if the parameter is truly constant.
    unsigned array_size = 1;

    for (unsigned k = 1; k < param_array->size(); ++k) {
      if (param_array->Data()[k] != param_array->Data()[0]) {
        array_size = param_array->size();
        break;
      }
    }

    v8::Local<v8::ArrayBuffer> array_buffer =
        v8::ArrayBuffer::New(isolate, array_size * sizeof(float));
    v8::Local<v8::Float32Array> float32_array =
        v8::Float32Array::New(array_buffer, 0, array_size);
    bool success;
    if (!param_values
             ->CreateDataProperty(current_context,
                                  V8String(isolate, param_name.IsolatedCopy()),
                                  float32_array)
             .To(&success)) {
      return false;
    }
    const v8::ArrayBuffer::Contents& contents = array_buffer->GetContents();
    memcpy(contents.Data(), param_array->Data(), array_size * sizeof(float));
  }

  // Perform JS function process() in AudioWorkletProcessor instance. The actual
  // V8 operation happens here to make the AudioWorkletProcessor class a thin
  // wrapper of v8::Object instance.
  ScriptValue result;
  if (!definition->ProcessFunction()
           ->Invoke(processor, ScriptValue(isolate, inputs),
                    ScriptValue(isolate, outputs),
                    ScriptValue(isolate, param_values))
           .To(&result)) {
    // process() method call failed for some reason or an exception was thrown
    // by the user supplied code. Disable the processor to exclude it from the
    // subsequent rendering task.
    processor->SetErrorState(AudioWorkletProcessorErrorState::kProcessError);
    return false;
  }

  // Copy |sequence<sequence<Float32Array>>| back to the original
  // |Vector<AudioBus*>|. While iterating, we also check if the size of backing
  // array buffer is changed. When the size does not match, silence the buffer.
  for (uint32_t output_bus_index = 0; output_bus_index < output_buses->size();
       ++output_bus_index) {
    AudioBus* output_bus = (*output_buses)[output_bus_index];
    for (uint32_t channel_index = 0;
         channel_index < output_bus->NumberOfChannels(); ++channel_index) {
      const v8::ArrayBuffer::Contents& contents =
          output_array_buffers[output_bus_index][channel_index]->GetContents();
      const size_t size = output_bus->length() * sizeof(float);
      if (contents.ByteLength() == size) {
        memcpy(output_bus->Channel(channel_index)->MutableData(),
               contents.Data(), size);
      } else {
        memset(output_bus->Channel(channel_index)->MutableData(), 0, size);
      }
    }
  }

  // Return the value from the user-supplied |process()| function. It is
  // used to maintain the lifetime of the node and the processor.
  DCHECK(!try_catch.HasCaught());
  return result.V8Value()->IsTrue();
}

AudioWorkletProcessorDefinition* AudioWorkletGlobalScope::FindDefinition(
    const String& name) {
  return processor_definition_map_.at(name);
}

unsigned AudioWorkletGlobalScope::NumberOfRegisteredDefinitions() {
  return processor_definition_map_.size();
}

std::unique_ptr<Vector<CrossThreadAudioWorkletProcessorInfo>>
AudioWorkletGlobalScope::WorkletProcessorInfoListForSynchronization() {
  auto processor_info_list =
      std::make_unique<Vector<CrossThreadAudioWorkletProcessorInfo>>();
  for (auto definition_entry : processor_definition_map_) {
    if (!definition_entry.value->IsSynchronized()) {
      definition_entry.value->MarkAsSynchronized();
      processor_info_list->emplace_back(*definition_entry.value);
    }
  }
  return processor_info_list;
}

ProcessorCreationParams* AudioWorkletGlobalScope::GetProcessorCreationParams() {
  return processor_creation_params_.get();
}

void AudioWorkletGlobalScope::SetCurrentFrame(size_t current_frame) {
  current_frame_ = current_frame;
}

void AudioWorkletGlobalScope::SetSampleRate(float sample_rate) {
  sample_rate_ = sample_rate;
}

double AudioWorkletGlobalScope::currentTime() const {
  return sample_rate_ > 0.0
        ? current_frame_ / static_cast<double>(sample_rate_)
        : 0.0;
}

void AudioWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(processor_definition_map_);
  visitor->Trace(processor_instances_);
  WorkletGlobalScope::Trace(visitor);
}

}  // namespace blink
