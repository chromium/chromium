// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_worklet_processor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_blink_audio_worklet_process_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_blink_audio_worklet_processor_constructor.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_object_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_definition.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

AudioWorkletGlobalScope::AudioWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread)
    : WorkletGlobalScope(std::move(creation_params),
                         thread->GetWorkerReportingProxy(),
                         thread) {
  // Audio is prone to jank introduced by e.g. the garbage collector. Workers
  // are generally put in a background mode (as they are non-visible). Audio is
  // an exception here, requiring low-latency behavior similar to any visible
  // state.
  GetThread()->GetWorkerBackingThread().SetForegrounded();
}

AudioWorkletGlobalScope::~AudioWorkletGlobalScope() = default;

void AudioWorkletGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  object_proxy_ = nullptr;
  is_closing_ = true;
  WorkletGlobalScope::Dispose();
}

void AudioWorkletGlobalScope::registerProcessor(
    const String& name,
    V8BlinkAudioWorkletProcessorConstructor* processor_ctor,
    ExceptionState& exception_state) {
  DCHECK(IsContextThread());

  // 1. If name is an empty string, throw a NotSupportedError.
  if (name.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The processor name cannot be empty.");
    return;
  }

  // 2. If name already exists as a key in the node name to processor
  //    constructor map, throw a NotSupportedError.
  if (processor_definition_map_.Contains(name)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "An AudioWorkletProcessor with name:\"" +
                                          name + "\" is already registered.");
    return;
  }

  // 3. If the result of IsConstructor(argument=processorCtor) is false, throw
  //    a TypeError .
  if (!processor_ctor->IsConstructor()) {
    exception_state.ThrowTypeError(
        "The provided class definition of \"" + name +
        "\" AudioWorkletProcessor is not a constructor.");
    return;
  }

  // 4. Let prototype be the result of Get(O=processorCtor, P="prototype").
  // 5. If the result of Type(argument=prototype) is not Object, throw a
  //    TypeError .
  CallbackMethodRetriever retriever(processor_ctor);
  retriever.GetPrototypeObject(exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // TODO(crbug.com/1077911): Do not extract process() function at the
  // registration step.
  v8::Local<v8::Function> v8_process =
      retriever.GetMethodOrThrow("process", exception_state);
  if (exception_state.HadException()) {
    return;
  }
  V8BlinkAudioWorkletProcessCallback* process =
      V8BlinkAudioWorkletProcessCallback::Create(v8_process);

  // The sufficient information to build a AudioWorkletProcessorDefinition
  // is collected. The rest of registration process is optional.
  // (i.e. parameterDescriptors)
  AudioWorkletProcessorDefinition* definition =
      AudioWorkletProcessorDefinition::Create(name, processor_ctor, process);

  v8::Isolate* isolate = processor_ctor->GetIsolate();
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();

  v8::Local<v8::Value> v8_parameter_descriptors;
  {
    TryRethrowScope rethrow_scope(isolate, exception_state);
    if (!processor_ctor->CallbackObject()
             ->Get(current_context,
                   V8AtomicString(isolate, "parameterDescriptors"))
             .ToLocal(&v8_parameter_descriptors)) {
      return;
    }
  }

  // 7. If parameterDescriptorsValue is not undefined, execute the following
  //    steps:
  if (!v8_parameter_descriptors->IsNullOrUndefined()) {
    // 7.1. Let parameterDescriptorSequence be the result of the conversion
    //      from parameterDescriptorsValue to an IDL value of type
    //      sequence<AudioParamDescriptor>.
    const HeapVector<Member<AudioParamDescriptor>>& given_param_descriptors =
        NativeValueTraits<IDLSequence<AudioParamDescriptor>>::NativeValue(
            isolate, v8_parameter_descriptors, exception_state);
    if (exception_state.HadException()) {
      return;
    }

    // 7.2. Let paramNames be an empty Array.
    HeapVector<Member<AudioParamDescriptor>> sanitized_param_descriptors;

    // 7.3. For each descriptor of parameterDescriptorSequence:
    HashSet<String> sanitized_names;
    for (const auto& given_descriptor : given_param_descriptors) {
      const String new_param_name = given_descriptor->name();
      if (!sanitized_names.insert(new_param_name).is_new_entry) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotSupportedError,
            "Found a duplicate name \"" + new_param_name +
                "\" in parameterDescriptors() from the AudioWorkletProcessor " +
                "definition of \"" + name + "\".");
        return;
      }

      // TODO(crbug.com/1078546): The steps 7.3.3 ~ 7.3.6 are missing.

      sanitized_param_descriptors.push_back(given_descriptor);
    }

    definition->SetAudioParamDescriptors(sanitized_param_descriptors);
  }

  // 8. Append the key-value pair name → processorCtor to node name to
  //    processor constructor map of the associated AudioWorkletGlobalScope.
  processor_definition_map_.Set(name, definition);

  // 9. Queue a media element task to append the key-value pair name →
  // parameterDescriptorSequence to the node name to parameter descriptor map
  // of the associated BaseAudioContext.
  if (object_proxy_) {
    // TODO(crbug.com/1223178): `object_proxy_` is designed to outlive the
    // global scope, so we don't need to null check but the unit test is not
    // able to replicate the cross-thread messaging logic yet, so we skip this
    // call in unit tests.
    object_proxy_->SynchronizeProcessorInfoList();
  }
}

AudioWorkletProcessor* AudioWorkletGlobalScope::CreateProcessor(
    const String& name,
    MessagePortChannel message_port_channel,
    scoped_refptr<SerializedScriptValue> node_options) {
  DCHECK(IsContextThread());

  // The registered definition is already checked by AudioWorkletNode
  // construction process, so the `definition` here must be valid.
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
  // `processor_creation_params_` in the global scope to perform the
  // construction properly.
  base::AutoReset<std::unique_ptr<ProcessorCreationParams>>
      processor_creation_extra_param(
          &processor_creation_params_,
          std::make_unique<ProcessorCreationParams>(
              name, std::move(message_port_channel)));

  // Make sure that the transferred `node_options` is deserializable.
  // See https://crbug.com/1429681 for details.
  if (!node_options->CanDeserializeIn(this)) {
    AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "Transferred AudioWorkletNodeOptions could not be deserialized because "
        "it contains an object of a type not available in "
        "AudioWorkletGlobalScope. See https://crbug.com/1429681 for details."));
    return nullptr;
  }

  UnpackedSerializedScriptValue* unpacked_node_options =
      MakeGarbageCollected<UnpackedSerializedScriptValue>(
          std::move(node_options));
  ScriptValue deserialized_options(
      isolate, unpacked_node_options->Deserialize(isolate));

  ScriptValue instance;
  if (!definition->ConstructorFunction()->Construct(deserialized_options)
          .To(&instance)) {
    return nullptr;
  }

  // ToImplWithTypeCheck() may return nullptr when the type does not match.
  AudioWorkletProcessor* processor =
      V8AudioWorkletProcessor::ToWrappable(isolate, instance.V8Value());

  return processor;
}

AudioWorkletProcessorDefinition* AudioWorkletGlobalScope::FindDefinition(
    const String& name) {
  const auto it = processor_definition_map_.find(name);
  if (it == processor_definition_map_.end()) {
    return nullptr;
  }
  return it->value.Get();
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

std::unique_ptr<ProcessorCreationParams>
AudioWorkletGlobalScope::GetProcessorCreationParams() {
  return std::move(processor_creation_params_);
}

void AudioWorkletGlobalScope::SetCurrentFrame(size_t current_frame) {
  current_frame_ = current_frame;
}

void AudioWorkletGlobalScope::SetSampleRate(float sample_rate) {
  sample_rate_ = sample_rate;
}

double AudioWorkletGlobalScope::currentTime() const {
  return sample_rate_ > 0.0 ? current_frame_ / static_cast<double>(sample_rate_)
                            : 0.0;
}

void AudioWorkletGlobalScope::SetObjectProxy(
    AudioWorkletObjectProxy& object_proxy) {
  object_proxy_ = &object_proxy;
}

void AudioWorkletGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(processor_definition_map_);
  WorkletGlobalScope::Trace(visitor);
}

}  // namespace blink
