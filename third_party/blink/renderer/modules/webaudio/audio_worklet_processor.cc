// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_blink_audio_worklet_process_callback.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_definition.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

AudioWorkletProcessor* AudioWorkletProcessor::Create(
    ExecutionContext* context,
    ExceptionState& exception_state) {
  AudioWorkletGlobalScope* global_scope = To<AudioWorkletGlobalScope>(context);
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());

  // Get the stored initialization parameter from the global scope.
  std::unique_ptr<ProcessorCreationParams> params =
      global_scope->GetProcessorCreationParams();

  // `params` can be null if there's no matching AudioWorkletNode instance.
  // (e.g. invoking AudioWorkletProcessor directly in AudioWorkletGlobalScope)
  if (!params) {
    exception_state.ThrowTypeError(
        "Illegal invocation of AudioWorkletProcessor constructor.");
    return nullptr;
  }
  auto* port = MakeGarbageCollected<MessagePort>(*global_scope);
  port->Entangle(std::move(params->PortChannel()));
  return MakeGarbageCollected<AudioWorkletProcessor>(global_scope,
                                                     params->Name(), port);
}

AudioWorkletProcessor::AudioWorkletProcessor(
    AudioWorkletGlobalScope* global_scope,
    const String& name,
    MessagePort* port)
    : global_scope_(global_scope), processor_port_(port), name_(name) {
  InstanceCounters::IncrementCounter(
      InstanceCounters::kAudioWorkletProcessorCounter);
}

AudioWorkletProcessor::~AudioWorkletProcessor() {
  InstanceCounters::DecrementCounter(
      InstanceCounters::kAudioWorkletProcessorCounter);
}

bool AudioWorkletProcessor::Process(
    const Vector<scoped_refptr<AudioBus>>& inputs,
    Vector<scoped_refptr<AudioBus>>& outputs,
    const HashMap<String, std::unique_ptr<AudioFloatArray>>& param_value_map) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletProcessor::Process");

  DCHECK(global_scope_->IsContextThread());
  DCHECK(!hasErrorOccurred());

  ScriptState* script_state =
      global_scope_->ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  AudioWorkletProcessorDefinition* definition =
      global_scope_->FindDefinition(Name());

  // 1st JS arg `inputs_`. Compare `inputs` and `inputs_`. Then allocates the
  // data container if necessary.
  if (!PortTopologyMatches(isolate, context, inputs, inputs_)) {
    bool inputs_cloned_successfully =
        ClonePortTopology(isolate, context, inputs, inputs_,
                          input_array_buffers_);
    DCHECK(inputs_cloned_successfully);
    if (!inputs_cloned_successfully) {
      return false;
    }
  }
  DCHECK(!inputs_.IsEmpty());
  DCHECK(inputs_.Get(isolate)->IsArray());
  DCHECK_EQ(inputs_.Get(isolate)->Length(), inputs.size());
  DCHECK_EQ(input_array_buffers_.size(), inputs.size());

  // Copies `inputs` to the internal `input_array_buffers_`.
  CopyPortToArrayBuffers(isolate, inputs, input_array_buffers_);

  // 2nd JS arg `outputs_`. Compare `outputs` and `outputs_`. Then allocates the
  // data container if necessary.
  if (!PortTopologyMatches(isolate, context, outputs, outputs_)) {
    bool outputs_cloned_successfully =
        ClonePortTopology(isolate, context, outputs, outputs_,
                          output_array_buffers_);
    DCHECK(outputs_cloned_successfully);
    if (!outputs_cloned_successfully) {
      return false;
    }
  } else {
    // The reallocation was not needed, so the arrays need to be zeroed before
    // passing them to the author script.
    ZeroArrayBuffers(isolate, output_array_buffers_);
  }
  DCHECK(!outputs_.IsEmpty());
  DCHECK(outputs_.Get(isolate)->IsArray());
  DCHECK_EQ(outputs_.Get(isolate)->Length(), outputs.size());
  DCHECK_EQ(output_array_buffers_.size(), outputs.size());

  // 3rd JS arg `params_`. Compare `param_value_map` and `params_`. Then
  // allocates the data container if necessary.
  if (!ParamValueMapMatchesToParamsObject(isolate, context, param_value_map,
                                          params_)) {
    bool params_cloned_successfully =
        CloneParamValueMapToObject(isolate, context, param_value_map, params_);
    DCHECK(params_cloned_successfully);
    if (!params_cloned_successfully) {
      return false;
    }
  }
  DCHECK(!params_.IsEmpty());
  DCHECK(params_.Get(isolate)->IsObject());

  // Copies `param_value_map` to the internal `params_` object. This operation
  // could fail if the getter of parameterDescriptors is overridden by user code
  // and returns incompatible data. (crbug.com/1151069)
  if (!CopyParamValueMapToObject(isolate, context, param_value_map, params_)) {
    SetErrorState(AudioWorkletProcessorErrorState::kProcessError);
    return false;
  }

  // Performs the user-defined AudioWorkletProcessor.process() function.
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  ScriptValue result;
  {
    TRACE_EVENT0(
        TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
        "AudioWorkletProcessor::Process (author script execution)");
    if (!definition->ProcessFunction()
             ->Invoke(this, ScriptValue(isolate, inputs_.Get(isolate)),
                      ScriptValue(isolate, outputs_.Get(isolate)),
                      ScriptValue(isolate, params_.Get(isolate)))
             .To(&result)) {
      SetErrorState(AudioWorkletProcessorErrorState::kProcessError);
      return false;
    }
  }
  DCHECK(!try_catch.HasCaught());

  // Copies the resulting output from author script to `outputs`.
  CopyArrayBuffersToPort(isolate, output_array_buffers_, outputs);

  // Return the value from the user-supplied `.process()` function. It is
  // used to maintain the lifetime of the node and the processor.
  return result.V8Value()->IsTrue();
}

void AudioWorkletProcessor::SetErrorState(
    AudioWorkletProcessorErrorState error_state) {
  error_state_ = error_state;
}

AudioWorkletProcessorErrorState AudioWorkletProcessor::GetErrorState() const {
  return error_state_;
}

bool AudioWorkletProcessor::hasErrorOccurred() const {
  return error_state_ != AudioWorkletProcessorErrorState::kNoError;
}

MessagePort* AudioWorkletProcessor::port() const {
  return processor_port_.Get();
}

void AudioWorkletProcessor::Trace(Visitor* visitor) const {
  visitor->Trace(global_scope_);
  visitor->Trace(processor_port_);
  visitor->Trace(inputs_);
  visitor->Trace(outputs_);
  visitor->Trace(params_);
  visitor->Trace(input_array_buffers_);
  visitor->Trace(output_array_buffers_);
  ScriptWrappable::Trace(visitor);
}

bool AudioWorkletProcessor::PortTopologyMatches(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const Vector<scoped_refptr<AudioBus>>& audio_port_1,
    const TraceWrapperV8Reference<v8::Array>& audio_port_2) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletProcessor::Process (compare topology)");
  if (audio_port_2.IsEmpty()) {
    return false;
  }

  v8::Local<v8::Array> port_2_local = audio_port_2.Get(isolate);
  DCHECK(port_2_local->IsArray());

  // Two audio ports may have a different number of inputs or outputs. See
  // crbug.com/1202060
  if (audio_port_1.size() != port_2_local->Length()) {
    return false;
  }

  v8::TryCatch try_catch(isolate);

  v8::Local<v8::Value> value;
  uint32_t bus_index_counter = 0;
  for (const auto& audio_bus_1 : audio_port_1) {
    if (!port_2_local->Get(context, bus_index_counter).ToLocal(&value) ||
        !value->IsArray()) {
      return false;
    }

    // Compare the length of AudioBus1[i] from AudioPort1 and AudioBus2[i] from
    // AudioPort2.
    unsigned number_of_channels =
        audio_bus_1 ? audio_bus_1->NumberOfChannels() : 0;
    v8::Local<v8::Array> audio_bus_2 = value.As<v8::Array>();
    if (number_of_channels != audio_bus_2->Length()) {
      return false;
    }

    // If the channel count of AudioBus1[i] and AudioBus2[i] matches, then
    // iterate all the channels in AudioBus1[i] and see if any AudioChannel
    // is detached. (i.e. transferred to a different thread)
    for (uint32_t channel_index = 0; channel_index < audio_bus_2->Length();
         ++channel_index) {
      if (!audio_bus_2->Get(context, channel_index).ToLocal(&value) ||
          !value->IsFloat32Array()) {
        return false;
      }
      v8::Local<v8::Float32Array> float32_array = value.As<v8::Float32Array>();

      // If any array is transferred, we need to rebuild them.
      if (float32_array->ByteLength() == 0) {
        return false;
      }
    }

    bus_index_counter++;
  }

  return true;
}

bool AudioWorkletProcessor::FreezeAudioPort(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Array>& audio_port_array) {
  v8::TryCatch try_catch(isolate);

  bool port_frozen;
  if (!audio_port_array->SetIntegrityLevel(context, v8::IntegrityLevel::kFrozen)
           .To(&port_frozen)) {
    return false;
  }

  v8::Local<v8::Value> bus_value;
  for (uint32_t bus_index = 0; bus_index < audio_port_array->Length();
       ++bus_index) {
    if (!audio_port_array->Get(context, bus_index).ToLocal(&bus_value) ||
        !bus_value->IsObject()) {
      return false;
    }
    bool bus_frozen;
    if (!bus_value.As<v8::Object>()
             ->SetIntegrityLevel(context, v8::IntegrityLevel::kFrozen)
             .To(&bus_frozen)) {
      return false;
    }
  }

  return true;
}

bool AudioWorkletProcessor::ClonePortTopology(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const Vector<scoped_refptr<AudioBus>>& audio_port_1,
    TraceWrapperV8Reference<v8::Array>& audio_port_2,
    BackingArrayBuffers& array_buffers) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletProcessor::Process (clone topology)");

  v8::Local<v8::Array> new_port_array =
      v8::Array::New(isolate, audio_port_1.size());
  BackingArrayBuffers new_array_buffers;
  new_array_buffers.ReserveInitialCapacity(audio_port_1.size());

  v8::TryCatch try_catch(isolate);

  uint32_t bus_index = 0;
  for (const auto& audio_bus : audio_port_1) {
    unsigned number_of_channels =
        audio_bus ? audio_bus->NumberOfChannels() : 0;
    size_t bus_length = audio_bus ? audio_bus->length() : 0;
    v8::Local<v8::Array> new_audio_bus =
        v8::Array::New(isolate, number_of_channels);
    bool new_bus_added;
    if (!new_port_array
             ->CreateDataProperty(context, bus_index, new_audio_bus)
             .To(&new_bus_added)) {
      return false;
    }
    new_array_buffers.UncheckedAppend(
        HeapVector<TraceWrapperV8Reference<v8::ArrayBuffer>>());
    new_array_buffers.back().ReserveInitialCapacity(number_of_channels);

    for (uint32_t channel_index = 0; channel_index < number_of_channels;
         ++channel_index) {
      v8::Local<v8::ArrayBuffer> array_buffer =
          v8::ArrayBuffer::New(isolate, bus_length * sizeof(float));
      v8::Local<v8::Float32Array> float32_array =
          v8::Float32Array::New(array_buffer, 0, bus_length);
      bool new_channel_added;
      if (!new_audio_bus
               ->CreateDataProperty(context, channel_index, float32_array)
               .To(&new_channel_added)) {
        return false;
      }
      new_array_buffers.back().UncheckedAppend(
          TraceWrapperV8Reference<v8::ArrayBuffer>(isolate, array_buffer));
    }

    bus_index++;
  }

  if (!FreezeAudioPort(isolate, context, new_port_array)) {
    return false;
  }

  audio_port_2.Reset(isolate, new_port_array);
  array_buffers.swap(new_array_buffers);
  return true;
}

void AudioWorkletProcessor::CopyPortToArrayBuffers(
      v8::Isolate* isolate,
      const Vector<scoped_refptr<AudioBus>>& audio_port,
      BackingArrayBuffers& array_buffers) {
  DCHECK_EQ(audio_port.size(), array_buffers.size());

  for (uint32_t bus_index = 0; bus_index < audio_port.size(); ++bus_index) {
    const scoped_refptr<AudioBus>& audio_bus = audio_port[bus_index];
    size_t bus_length = audio_bus ? audio_bus->length() : 0;
    unsigned number_of_channels = audio_bus ? audio_bus->NumberOfChannels() : 0;
    for (uint32_t channel_index = 0; channel_index < number_of_channels;
         ++channel_index) {
      auto backing_store = array_buffers[bus_index][channel_index]
                               .Get(isolate)
                               ->GetBackingStore();
      memcpy(backing_store->Data(), audio_bus->Channel(channel_index)->Data(),
             bus_length * sizeof(float));
    }
  }
}

void AudioWorkletProcessor::CopyArrayBuffersToPort(
    v8::Isolate* isolate,
    const BackingArrayBuffers& array_buffers,
    Vector<scoped_refptr<AudioBus>>& audio_port) {
  DCHECK_EQ(array_buffers.size(), audio_port.size());

  for (uint32_t bus_index = 0; bus_index < audio_port.size(); ++bus_index) {
    const scoped_refptr<AudioBus>& audio_bus = audio_port[bus_index];
    for (uint32_t channel_index = 0;
         channel_index < audio_bus->NumberOfChannels(); ++channel_index) {
      auto backing_store = array_buffers[bus_index][channel_index]
                               .Get(isolate)
                               ->GetBackingStore();
      const size_t bus_length = audio_bus->length() * sizeof(float);

      // An ArrayBuffer might be transferred. So we need to check the byte
      // length and silence the output buffer if needed.
      if (backing_store->ByteLength() == bus_length) {
        memcpy(audio_bus->Channel(channel_index)->MutableData(),
               backing_store->Data(), bus_length);
      } else {
        memset(audio_bus->Channel(channel_index)->MutableData(), 0, bus_length);
      }
    }
  }
}

void AudioWorkletProcessor::ZeroArrayBuffers(
    v8::Isolate* isolate,
    const BackingArrayBuffers& array_buffers) {
  for (uint32_t bus_index = 0; bus_index < array_buffers.size(); ++bus_index) {
    for (uint32_t channel_index = 0;
         channel_index < array_buffers[bus_index].size(); ++channel_index) {
      auto backing_store = array_buffers[bus_index][channel_index]
                               .Get(isolate)
                               ->GetBackingStore();
      memset(backing_store->Data(), 0, backing_store->ByteLength());
    }
  }
}

bool AudioWorkletProcessor::ParamValueMapMatchesToParamsObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const HashMap<String, std::unique_ptr<AudioFloatArray>>& param_value_map,
    const TraceWrapperV8Reference<v8::Object>& params) {
  v8::TryCatch try_catch(isolate);

  if (params.IsEmpty()) {
    return false;
  }

  v8::Local<v8::Object> params_object = params.Get(isolate);

  for (const auto& entry : param_value_map) {
    const String param_name = entry.key;
    const auto* param_float_array = entry.value.get();
    v8::Local<v8::String> v8_param_name = V8String(isolate, param_name);

    // TODO(crbug.com/1095113): Remove this check and move the logic to
    // AudioWorkletHandler.
    unsigned array_size = 1;
    for (unsigned k = 1; k < param_float_array->size(); ++k) {
      if (param_float_array->Data()[k] != param_float_array->Data()[0]) {
        array_size = param_float_array->size();
        break;
      }
    }

    // The `param_name` should exist in the `param` object.
    v8::Local<v8::Value> param_array_value;
    if (!params_object->Get(context, v8_param_name)
             .ToLocal(&param_array_value) ||
        !param_array_value->IsFloat32Array()) {
      return false;
    }

    // If the detected array length doesn't match or any underlying array
    // buffer is transferred, we have to reallocate.
    v8::Local<v8::Float32Array> float32_array =
        param_array_value.As<v8::Float32Array>();
    if (float32_array->Length() != array_size ||
        float32_array->Buffer()->ByteLength() == 0) {
      return false;
    }
  }

  return true;
}

bool AudioWorkletProcessor::CloneParamValueMapToObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const HashMap<String, std::unique_ptr<AudioFloatArray>>& param_value_map,
    TraceWrapperV8Reference<v8::Object>& params) {
  TRACE_EVENT0(
      TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
      "AudioWorkletProcessor::Process (AudioParam memory allocation)");

  v8::TryCatch try_catch(isolate);

  v8::Local<v8::Object> new_params_object = v8::Object::New(isolate);

  for (const auto& entry : param_value_map) {
    const String param_name = entry.key;
    const auto* param_float_array = entry.value.get();
    v8::Local<v8::String> v8_param_name = V8String(isolate, param_name);

    // TODO(crbug.com/1095113): Remove this check and move the logic to
    // AudioWorkletHandler.
    unsigned array_size = 1;
    for (unsigned k = 1; k < param_float_array->size(); ++k) {
      if (param_float_array->Data()[k] != param_float_array->Data()[0]) {
        array_size = param_float_array->size();
        break;
      }
    }
    DCHECK(array_size == 1 || array_size == param_float_array->size());

    v8::Local<v8::ArrayBuffer> array_buffer =
        v8::ArrayBuffer::New(isolate, array_size * sizeof(float));
    v8::Local<v8::Float32Array> float32_array =
        v8::Float32Array::New(array_buffer, 0, array_size);
    bool new_param_array_created;
    if (!new_params_object
             ->CreateDataProperty(context, v8_param_name, float32_array)
             .To(&new_param_array_created)) {
      return false;
    }
  }

  bool object_frozen;
  if (!new_params_object
           ->SetIntegrityLevel(context, v8::IntegrityLevel::kFrozen)
           .To(&object_frozen)) {
    return false;
  }

  params.Reset(isolate, new_params_object);
  return true;
}

bool AudioWorkletProcessor::CopyParamValueMapToObject(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const HashMap<String, std::unique_ptr<AudioFloatArray>>& param_value_map,
    TraceWrapperV8Reference<v8::Object>& params) {
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::Object> params_object = params.Get(isolate);

  for (const auto& entry : param_value_map) {
    const String param_name = entry.key;
    const AudioFloatArray* param_array = entry.value.get();

    v8::Local<v8::Value> param_array_value;
    if (!params_object->Get(context, V8String(isolate, param_name))
                      .ToLocal(&param_array_value) ||
        !param_array_value->IsFloat32Array()) {
      return false;
    }

    v8::Local<v8::Float32Array> float32_array =
        param_array_value.As<v8::Float32Array>();
    size_t array_length = float32_array->Length();

    // The `float32_array` is neither 1 nor 128 frames, or the array buffer is
    // trasnferred/detached, do not proceed.
    if ((array_length != 1 && array_length != param_array->size()) ||
        float32_array->Buffer()->ByteLength() == 0) {
      return false;
    }

    memcpy(float32_array->Buffer()->GetBackingStore()->Data(),
           param_array->Data(), array_length * sizeof(float));
  }

  return true;
}

}  // namespace blink
