// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_v8.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_dictionary.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internal_dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_2.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_object.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_treat_non_object_as_null_void_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_callback_function.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/flexible_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static const v8::Eternal<v8::Name>* eternalV8TestDictionaryKeys(v8::Isolate* isolate) {
  static const char* const kKeys[] = {
    "anyInRecordMember",
    "anyMember",
    "applicableToTypeLongMember",
    "applicableToTypeStringMember",
    "booleanMember",
    "callbackFunctionMember",
    "create",
    "deprecatedCreateMember",
    "dictionaryMember",
    "domStringTreatNullAsEmptyStringMember",
    "doubleOrNullMember",
    "doubleOrNullOrDoubleOrNullSequenceMember",
    "doubleOrNullRecordMember",
    "doubleOrNullSequenceMember",
    "doubleOrStringMember",
    "doubleOrStringSequenceMember",
    "elementOrNullMember",
    "elementOrNullRecordMember",
    "elementOrNullSequenceMember",
    "enumMember",
    "enumOrNullMember",
    "enumSequenceMember",
    "eventTargetMember",
    "garbageCollectedRecordMember",
    "internalDictionarySequenceMember",
    "longMember",
    "member-with-hyphen-in-name",
    "objectMember",
    "objectOrNullMember",
    "originTrialMember",
    "originTrialSecondMember",
    "otherDoubleOrStringMember",
    "public",
    "recordMember",
    "requiredCallbackFunctionMember",
    "restrictedDoubleMember",
    "runtimeMember",
    "runtimeSecondMember",
    "stringMember",
    "stringOrNullMember",
    "stringOrNullRecordMember",
    "stringOrNullSequenceMember",
    "stringSequenceMember",
    "testEnumOrNullOrTestEnumSequenceMember",
    "testEnumOrTestEnumOrNullSequenceMember",
    "testEnumOrTestEnumSequenceMember",
    "testInterface2OrUint8ArrayMember",
    "testInterfaceMember",
    "testInterfaceOrNullMember",
    "testInterfaceSequenceMember",
    "testObjectSequenceMember",
    "treatNonNullObjMember",
    "treatNullAsStringSequenceMember",
    "uint8ArrayMember",
    "unionInRecordMember",
    "unionMemberWithSequenceDefault",
    "unionOrNullRecordMember",
    "unionOrNullSequenceMember",
    "unionWithAnnotatedTypeMember",
    "unionWithTypedefs",
    "unrestrictedDoubleMember",
    "usvStringOrNullMember",
  };
  return V8PerIsolateData::From(isolate)->FindOrCreateEternalNameCache(
      kKeys, kKeys, base::size(kKeys));
}

void V8TestDictionary::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8_value, TestDictionary* impl, ExceptionState& exception_state) {
  if (IsUndefinedOrNull(v8_value)) {
    exception_state.ThrowTypeError("Missing required member(s): requiredCallbackFunctionMember.");
    return;
  }
  if (!v8_value->IsObject()) {
    exception_state.ThrowTypeError("cannot convert to dictionary.");
    return;
  }
  v8::Local<v8::Object> v8Object = v8_value.As<v8::Object>();
  ALLOW_UNUSED_LOCAL(v8Object);

  const v8::Eternal<v8::Name>* keys = eternalV8TestDictionaryKeys(isolate);
  v8::TryCatch block(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  ExecutionContext* executionContext = ToExecutionContext(context);
  DCHECK(executionContext);
  v8::Local<v8::Value> any_in_record_member_value;
  if (!v8Object->Get(context, keys[0].Get(isolate)).ToLocal(&any_in_record_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (any_in_record_member_value.IsEmpty() || any_in_record_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<std::pair<String, ScriptValue>> any_in_record_member_cpp_value = NativeValueTraits<IDLRecord<IDLString, ScriptValue>>::NativeValue(isolate, any_in_record_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setAnyInRecordMember(any_in_record_member_cpp_value);
  }

  v8::Local<v8::Value> any_member_value;
  if (!v8Object->Get(context, keys[1].Get(isolate)).ToLocal(&any_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (any_member_value.IsEmpty() || any_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    ScriptValue any_member_cpp_value = ScriptValue(isolate, any_member_value);
    impl->setAnyMember(any_member_cpp_value);
  }

  v8::Local<v8::Value> applicable_to_type_long_member_value;
  if (!v8Object->Get(context, keys[2].Get(isolate)).ToLocal(&applicable_to_type_long_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (applicable_to_type_long_member_value.IsEmpty() || applicable_to_type_long_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    int32_t applicable_to_type_long_member_cpp_value = NativeValueTraits<IDLLongClamp>::NativeValue(isolate, applicable_to_type_long_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setApplicableToTypeLongMember(applicable_to_type_long_member_cpp_value);
  }

  v8::Local<v8::Value> applicable_to_type_string_member_value;
  if (!v8Object->Get(context, keys[3].Get(isolate)).ToLocal(&applicable_to_type_string_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (applicable_to_type_string_member_value.IsEmpty() || applicable_to_type_string_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<kTreatNullAsEmptyString> applicable_to_type_string_member_cpp_value = applicable_to_type_string_member_value;
    if (!applicable_to_type_string_member_cpp_value.Prepare(exception_state))
      return;
    impl->setApplicableToTypeStringMember(applicable_to_type_string_member_cpp_value);
  }

  v8::Local<v8::Value> boolean_member_value;
  if (!v8Object->Get(context, keys[4].Get(isolate)).ToLocal(&boolean_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (boolean_member_value.IsEmpty() || boolean_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    bool boolean_member_cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(isolate, boolean_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setBooleanMember(boolean_member_cpp_value);
  }

  v8::Local<v8::Value> callback_function_member_value;
  if (!v8Object->Get(context, keys[5].Get(isolate)).ToLocal(&callback_function_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (callback_function_member_value.IsEmpty() || callback_function_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    if (!callback_function_member_value->IsFunction()) {
      exception_state.ThrowTypeError("member callbackFunctionMember is not a function.");
      return;
    }
    V8VoidCallbackFunction* callback_function_member_cpp_value = V8VoidCallbackFunction::Create(callback_function_member_value.As<v8::Function>());
    impl->setCallbackFunctionMember(callback_function_member_cpp_value);
  }

  v8::Local<v8::Value> create_value;
  if (!v8Object->Get(context, keys[6].Get(isolate)).ToLocal(&create_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (create_value.IsEmpty() || create_value->IsUndefined()) {
    // Do nothing.
  } else {
    bool create_cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(isolate, create_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setCreateMember(create_cpp_value);
  }

  v8::Local<v8::Value> deprecated_create_member_value;
  if (!v8Object->Get(context, keys[7].Get(isolate)).ToLocal(&deprecated_create_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (deprecated_create_member_value.IsEmpty() || deprecated_create_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    Deprecation::CountDeprecation(CurrentExecutionContext(isolate), WebFeature::kCreateMember);
    bool deprecated_create_member_cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(isolate, deprecated_create_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setCreateMember(deprecated_create_member_cpp_value);
  }

  v8::Local<v8::Value> dictionary_member_value;
  if (!v8Object->Get(context, keys[8].Get(isolate)).ToLocal(&dictionary_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (dictionary_member_value.IsEmpty() || dictionary_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    Dictionary dictionary_member_cpp_value = NativeValueTraits<Dictionary>::NativeValue(isolate, dictionary_member_value, exception_state);
    if (exception_state.HadException())
      return;
    if (!dictionary_member_cpp_value.IsObject()) {
      exception_state.ThrowTypeError("member dictionaryMember is not an object.");
      return;
    }
    impl->setDictionaryMember(dictionary_member_cpp_value);
  }

  v8::Local<v8::Value> dom_string_treat_null_as_empty_string_member_value;
  if (!v8Object->Get(context, keys[9].Get(isolate)).ToLocal(&dom_string_treat_null_as_empty_string_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (dom_string_treat_null_as_empty_string_member_value.IsEmpty() || dom_string_treat_null_as_empty_string_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<kTreatNullAsEmptyString> dom_string_treat_null_as_empty_string_member_cpp_value = dom_string_treat_null_as_empty_string_member_value;
    if (!dom_string_treat_null_as_empty_string_member_cpp_value.Prepare(exception_state))
      return;
    impl->setDomStringTreatNullAsEmptyStringMember(dom_string_treat_null_as_empty_string_member_cpp_value);
  }

  v8::Local<v8::Value> double_or_null_member_value;
  if (!v8Object->Get(context, keys[10].Get(isolate)).ToLocal(&double_or_null_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (double_or_null_member_value.IsEmpty() || double_or_null_member_value->IsUndefined()) {
    // Do nothing.
  } else if (double_or_null_member_value->IsNull()) {
    impl->setDoubleOrNullMemberToNull();
  } else {
    double double_or_null_member_cpp_value = NativeValueTraits<IDLDouble>::NativeValue(isolate, double_or_null_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setDoubleOrNullMember(double_or_null_member_cpp_value);
  }

  v8::Local<v8::Value> double_or_null_or_double_or_null_sequence_member_value;
  if (!v8Object->Get(context, keys[11].Get(isolate)).ToLocal(&double_or_null_or_double_or_null_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (double_or_null_or_double_or_null_sequence_member_value.IsEmpty() || double_or_null_or_double_or_null_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    DoubleOrDoubleOrNullSequence double_or_null_or_double_or_null_sequence_member_cpp_value;
    V8DoubleOrDoubleOrNullSequence::ToImpl(isolate, double_or_null_or_double_or_null_sequence_member_value, double_or_null_or_double_or_null_sequence_member_cpp_value, UnionTypeConversionMode::kNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setDoubleOrNullOrDoubleOrNullSequenceMember(double_or_null_or_double_or_null_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> double_or_null_record_member_value;
  if (!v8Object->Get(context, keys[12].Get(isolate)).ToLocal(&double_or_null_record_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (double_or_null_record_member_value.IsEmpty() || double_or_null_record_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<std::pair<String, base::Optional<double>>> double_or_null_record_member_cpp_value = NativeValueTraits<IDLRecord<IDLString, IDLNullable<IDLDouble>>>::NativeValue(isolate, double_or_null_record_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setDoubleOrNullRecordMember(double_or_null_record_member_cpp_value);
  }

  v8::Local<v8::Value> double_or_null_sequence_member_value;
  if (!v8Object->Get(context, keys[13].Get(isolate)).ToLocal(&double_or_null_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (double_or_null_sequence_member_value.IsEmpty() || double_or_null_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<base::Optional<double>> double_or_null_sequence_member_cpp_value = NativeValueTraits<IDLSequence<IDLNullable<IDLDouble>>>::NativeValue(isolate, double_or_null_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setDoubleOrNullSequenceMember(double_or_null_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> double_or_string_member_value;
  if (!v8Object->Get(context, keys[14].Get(isolate)).ToLocal(&double_or_string_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (double_or_string_member_value.IsEmpty() || double_or_string_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    DoubleOrString double_or_string_member_cpp_value;
    V8DoubleOrString::ToImpl(isolate, double_or_string_member_value, double_or_string_member_cpp_value, UnionTypeConversionMode::kNotNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setDoubleOrStringMember(double_or_string_member_cpp_value);
  }

  v8::Local<v8::Value> double_or_string_sequence_member_value;
  if (!v8Object->Get(context, keys[15].Get(isolate)).ToLocal(&double_or_string_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (double_or_string_sequence_member_value.IsEmpty() || double_or_string_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<DoubleOrString> double_or_string_sequence_member_cpp_value = NativeValueTraits<IDLSequence<DoubleOrString>>::NativeValue(isolate, double_or_string_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setDoubleOrStringSequenceMember(double_or_string_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> element_or_null_member_value;
  if (!v8Object->Get(context, keys[16].Get(isolate)).ToLocal(&element_or_null_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (element_or_null_member_value.IsEmpty() || element_or_null_member_value->IsUndefined()) {
    // Do nothing.
  } else if (element_or_null_member_value->IsNull()) {
    impl->setElementOrNullMemberToNull();
  } else {
    Element* element_or_null_member_cpp_value = V8Element::ToImplWithTypeCheck(isolate, element_or_null_member_value);
    if (!element_or_null_member_cpp_value) {
      exception_state.ThrowTypeError("member elementOrNullMember is not of type Element.");
      return;
    }
    impl->setElementOrNullMember(element_or_null_member_cpp_value);
  }

  v8::Local<v8::Value> element_or_null_record_member_value;
  if (!v8Object->Get(context, keys[17].Get(isolate)).ToLocal(&element_or_null_record_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (element_or_null_record_member_value.IsEmpty() || element_or_null_record_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<std::pair<String, Member<Element>>> element_or_null_record_member_cpp_value = NativeValueTraits<IDLRecord<IDLString, IDLNullable<Element>>>::NativeValue(isolate, element_or_null_record_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setElementOrNullRecordMember(element_or_null_record_member_cpp_value);
  }

  v8::Local<v8::Value> element_or_null_sequence_member_value;
  if (!v8Object->Get(context, keys[18].Get(isolate)).ToLocal(&element_or_null_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (element_or_null_sequence_member_value.IsEmpty() || element_or_null_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<Member<Element>> element_or_null_sequence_member_cpp_value = NativeValueTraits<IDLSequence<IDLNullable<Element>>>::NativeValue(isolate, element_or_null_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setElementOrNullSequenceMember(element_or_null_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> enum_member_value;
  if (!v8Object->Get(context, keys[19].Get(isolate)).ToLocal(&enum_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (enum_member_value.IsEmpty() || enum_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<> enum_member_cpp_value = enum_member_value;
    if (!enum_member_cpp_value.Prepare(exception_state))
      return;
    const char* const kValidValues[] = {
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(enum_member_cpp_value, kValidValues, base::size(kValidValues), "TestEnum", exception_state))
      return;
    impl->setEnumMember(enum_member_cpp_value);
  }

  v8::Local<v8::Value> enum_or_null_member_value;
  if (!v8Object->Get(context, keys[20].Get(isolate)).ToLocal(&enum_or_null_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (enum_or_null_member_value.IsEmpty() || enum_or_null_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<kTreatNullAndUndefinedAsNullString> enum_or_null_member_cpp_value = enum_or_null_member_value;
    if (!enum_or_null_member_cpp_value.Prepare(exception_state))
      return;
    const char* const kValidValues[] = {
        nullptr,
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(enum_or_null_member_cpp_value, kValidValues, base::size(kValidValues), "TestEnum", exception_state))
      return;
    impl->setEnumOrNullMember(enum_or_null_member_cpp_value);
  }

  v8::Local<v8::Value> enum_sequence_member_value;
  if (!v8Object->Get(context, keys[21].Get(isolate)).ToLocal(&enum_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (enum_sequence_member_value.IsEmpty() || enum_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<String> enum_sequence_member_cpp_value = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(isolate, enum_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    const char* const kValidValues[] = {
        "",
        "EnumValue1",
        "EnumValue2",
        "EnumValue3",
    };
    if (!IsValidEnum(enum_sequence_member_cpp_value, kValidValues, base::size(kValidValues), "TestEnum", exception_state))
      return;
    impl->setEnumSequenceMember(enum_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> event_target_member_value;
  if (!v8Object->Get(context, keys[22].Get(isolate)).ToLocal(&event_target_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (event_target_member_value.IsEmpty() || event_target_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    EventTarget* event_target_member_cpp_value = V8EventTarget::ToImplWithTypeCheck(isolate, event_target_member_value);
    if (!event_target_member_cpp_value) {
      exception_state.ThrowTypeError("member eventTargetMember is not of type EventTarget.");
      return;
    }
    impl->setEventTargetMember(event_target_member_cpp_value);
  }

  v8::Local<v8::Value> garbage_collected_record_member_value;
  if (!v8Object->Get(context, keys[23].Get(isolate)).ToLocal(&garbage_collected_record_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (garbage_collected_record_member_value.IsEmpty() || garbage_collected_record_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<std::pair<String, Member<TestObject>>> garbage_collected_record_member_cpp_value = NativeValueTraits<IDLRecord<IDLUSVString, TestObject>>::NativeValue(isolate, garbage_collected_record_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setGarbageCollectedRecordMember(garbage_collected_record_member_cpp_value);
  }

  v8::Local<v8::Value> internal_dictionary_sequence_member_value;
  if (!v8Object->Get(context, keys[24].Get(isolate)).ToLocal(&internal_dictionary_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (internal_dictionary_sequence_member_value.IsEmpty() || internal_dictionary_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<Member<InternalDictionary>> internal_dictionary_sequence_member_cpp_value = NativeValueTraits<IDLSequence<InternalDictionary>>::NativeValue(isolate, internal_dictionary_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setInternalDictionarySequenceMember(internal_dictionary_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> long_member_value;
  if (!v8Object->Get(context, keys[25].Get(isolate)).ToLocal(&long_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (long_member_value.IsEmpty() || long_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    int32_t long_member_cpp_value = NativeValueTraits<IDLLong>::NativeValue(isolate, long_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setLongMember(long_member_cpp_value);
  }

  v8::Local<v8::Value> member_with_hyphen_in_name_value;
  if (!v8Object->Get(context, keys[26].Get(isolate)).ToLocal(&member_with_hyphen_in_name_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (member_with_hyphen_in_name_value.IsEmpty() || member_with_hyphen_in_name_value->IsUndefined()) {
    // Do nothing.
  } else {
    bool member_with_hyphen_in_name_cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(isolate, member_with_hyphen_in_name_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setMemberWithHyphenInName(member_with_hyphen_in_name_cpp_value);
  }

  v8::Local<v8::Value> object_member_value;
  if (!v8Object->Get(context, keys[27].Get(isolate)).ToLocal(&object_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (object_member_value.IsEmpty() || object_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    ScriptValue object_member_cpp_value = ScriptValue(isolate, object_member_value);
    if (!object_member_cpp_value.IsObject()) {
      exception_state.ThrowTypeError("member objectMember is not an object.");
      return;
    }
    impl->setObjectMember(object_member_cpp_value);
  }

  v8::Local<v8::Value> object_or_null_member_value;
  if (!v8Object->Get(context, keys[28].Get(isolate)).ToLocal(&object_or_null_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (object_or_null_member_value.IsEmpty() || object_or_null_member_value->IsUndefined()) {
    // Do nothing.
  } else if (object_or_null_member_value->IsNull()) {
    impl->setObjectOrNullMemberToNull();
  } else {
    ScriptValue object_or_null_member_cpp_value = ScriptValue(isolate, object_or_null_member_value);
    if (!object_or_null_member_cpp_value.IsObject()) {
      exception_state.ThrowTypeError("member objectOrNullMember is not an object.");
      return;
    }
    impl->setObjectOrNullMember(object_or_null_member_cpp_value);
  }

  v8::Local<v8::Value> other_double_or_string_member_value;
  if (!v8Object->Get(context, keys[31].Get(isolate)).ToLocal(&other_double_or_string_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (other_double_or_string_member_value.IsEmpty() || other_double_or_string_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    DoubleOrString other_double_or_string_member_cpp_value;
    V8DoubleOrString::ToImpl(isolate, other_double_or_string_member_value, other_double_or_string_member_cpp_value, UnionTypeConversionMode::kNotNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setOtherDoubleOrStringMember(other_double_or_string_member_cpp_value);
  }

  v8::Local<v8::Value> public_value;
  if (!v8Object->Get(context, keys[32].Get(isolate)).ToLocal(&public_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (public_value.IsEmpty() || public_value->IsUndefined()) {
    // Do nothing.
  } else {
    bool public_cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(isolate, public_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setIsPublic(public_cpp_value);
  }

  v8::Local<v8::Value> record_member_value;
  if (!v8Object->Get(context, keys[33].Get(isolate)).ToLocal(&record_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (record_member_value.IsEmpty() || record_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<std::pair<String, int8_t>> record_member_cpp_value = NativeValueTraits<IDLRecord<IDLByteString, IDLByte>>::NativeValue(isolate, record_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setRecordMember(record_member_cpp_value);
  }

  v8::Local<v8::Value> required_callback_function_member_value;
  if (!v8Object->Get(context, keys[34].Get(isolate)).ToLocal(&required_callback_function_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (required_callback_function_member_value.IsEmpty() || required_callback_function_member_value->IsUndefined()) {
    exception_state.ThrowTypeError("required member requiredCallbackFunctionMember is undefined.");
    return;
  } else {
    if (!required_callback_function_member_value->IsFunction()) {
      exception_state.ThrowTypeError("member requiredCallbackFunctionMember is not a function.");
      return;
    }
    V8VoidCallbackFunction* required_callback_function_member_cpp_value = V8VoidCallbackFunction::Create(required_callback_function_member_value.As<v8::Function>());
    impl->setRequiredCallbackFunctionMember(required_callback_function_member_cpp_value);
  }

  v8::Local<v8::Value> restricted_double_member_value;
  if (!v8Object->Get(context, keys[35].Get(isolate)).ToLocal(&restricted_double_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (restricted_double_member_value.IsEmpty() || restricted_double_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    double restricted_double_member_cpp_value = NativeValueTraits<IDLDouble>::NativeValue(isolate, restricted_double_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setRestrictedDoubleMember(restricted_double_member_cpp_value);
  }

  v8::Local<v8::Value> string_member_value;
  if (!v8Object->Get(context, keys[38].Get(isolate)).ToLocal(&string_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (string_member_value.IsEmpty() || string_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<> string_member_cpp_value = string_member_value;
    if (!string_member_cpp_value.Prepare(exception_state))
      return;
    impl->setStringMember(string_member_cpp_value);
  }

  v8::Local<v8::Value> string_or_null_member_value;
  if (!v8Object->Get(context, keys[39].Get(isolate)).ToLocal(&string_or_null_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (string_or_null_member_value.IsEmpty() || string_or_null_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<kTreatNullAndUndefinedAsNullString> string_or_null_member_cpp_value = string_or_null_member_value;
    if (!string_or_null_member_cpp_value.Prepare(exception_state))
      return;
    impl->setStringOrNullMember(string_or_null_member_cpp_value);
  }

  v8::Local<v8::Value> string_or_null_record_member_value;
  if (!v8Object->Get(context, keys[40].Get(isolate)).ToLocal(&string_or_null_record_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (string_or_null_record_member_value.IsEmpty() || string_or_null_record_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<std::pair<String, String>> string_or_null_record_member_cpp_value = NativeValueTraits<IDLRecord<IDLString, IDLStringOrNull>>::NativeValue(isolate, string_or_null_record_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setStringOrNullRecordMember(string_or_null_record_member_cpp_value);
  }

  v8::Local<v8::Value> string_or_null_sequence_member_value;
  if (!v8Object->Get(context, keys[41].Get(isolate)).ToLocal(&string_or_null_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (string_or_null_sequence_member_value.IsEmpty() || string_or_null_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<String> string_or_null_sequence_member_cpp_value = NativeValueTraits<IDLSequence<IDLStringOrNull>>::NativeValue(isolate, string_or_null_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setStringOrNullSequenceMember(string_or_null_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> string_sequence_member_value;
  if (!v8Object->Get(context, keys[42].Get(isolate)).ToLocal(&string_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (string_sequence_member_value.IsEmpty() || string_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<String> string_sequence_member_cpp_value = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(isolate, string_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setStringSequenceMember(string_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> test_enum_or_null_or_test_enum_sequence_member_value;
  if (!v8Object->Get(context, keys[43].Get(isolate)).ToLocal(&test_enum_or_null_or_test_enum_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (test_enum_or_null_or_test_enum_sequence_member_value.IsEmpty() || test_enum_or_null_or_test_enum_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    TestEnumOrTestEnumSequence test_enum_or_null_or_test_enum_sequence_member_cpp_value;
    V8TestEnumOrTestEnumSequence::ToImpl(isolate, test_enum_or_null_or_test_enum_sequence_member_value, test_enum_or_null_or_test_enum_sequence_member_cpp_value, UnionTypeConversionMode::kNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setTestEnumOrNullOrTestEnumSequenceMember(test_enum_or_null_or_test_enum_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> test_enum_or_test_enum_or_null_sequence_member_value;
  if (!v8Object->Get(context, keys[44].Get(isolate)).ToLocal(&test_enum_or_test_enum_or_null_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (test_enum_or_test_enum_or_null_sequence_member_value.IsEmpty() || test_enum_or_test_enum_or_null_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    TestEnumOrTestEnumOrNullSequence test_enum_or_test_enum_or_null_sequence_member_cpp_value;
    V8TestEnumOrTestEnumOrNullSequence::ToImpl(isolate, test_enum_or_test_enum_or_null_sequence_member_value, test_enum_or_test_enum_or_null_sequence_member_cpp_value, UnionTypeConversionMode::kNotNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setTestEnumOrTestEnumOrNullSequenceMember(test_enum_or_test_enum_or_null_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> test_enum_or_test_enum_sequence_member_value;
  if (!v8Object->Get(context, keys[45].Get(isolate)).ToLocal(&test_enum_or_test_enum_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (test_enum_or_test_enum_sequence_member_value.IsEmpty() || test_enum_or_test_enum_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    TestEnumOrTestEnumSequence test_enum_or_test_enum_sequence_member_cpp_value;
    V8TestEnumOrTestEnumSequence::ToImpl(isolate, test_enum_or_test_enum_sequence_member_value, test_enum_or_test_enum_sequence_member_cpp_value, UnionTypeConversionMode::kNotNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setTestEnumOrTestEnumSequenceMember(test_enum_or_test_enum_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> test_interface_2_or_uint8_array_member_value;
  if (!v8Object->Get(context, keys[46].Get(isolate)).ToLocal(&test_interface_2_or_uint8_array_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (test_interface_2_or_uint8_array_member_value.IsEmpty() || test_interface_2_or_uint8_array_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    TestInterface2OrUint8Array test_interface_2_or_uint8_array_member_cpp_value;
    V8TestInterface2OrUint8Array::ToImpl(isolate, test_interface_2_or_uint8_array_member_value, test_interface_2_or_uint8_array_member_cpp_value, UnionTypeConversionMode::kNotNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setTestInterface2OrUint8ArrayMember(test_interface_2_or_uint8_array_member_cpp_value);
  }

  v8::Local<v8::Value> test_interface_member_value;
  if (!v8Object->Get(context, keys[47].Get(isolate)).ToLocal(&test_interface_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (test_interface_member_value.IsEmpty() || test_interface_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    TestInterfaceImplementation* test_interface_member_cpp_value = V8TestInterface::ToImplWithTypeCheck(isolate, test_interface_member_value);
    if (!test_interface_member_cpp_value) {
      exception_state.ThrowTypeError("member testInterfaceMember is not of type TestInterface.");
      return;
    }
    impl->setTestInterfaceMember(test_interface_member_cpp_value);
  }

  v8::Local<v8::Value> test_interface_or_null_member_value;
  if (!v8Object->Get(context, keys[48].Get(isolate)).ToLocal(&test_interface_or_null_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (test_interface_or_null_member_value.IsEmpty() || test_interface_or_null_member_value->IsUndefined()) {
    // Do nothing.
  } else if (test_interface_or_null_member_value->IsNull()) {
    impl->setTestInterfaceOrNullMemberToNull();
  } else {
    TestInterfaceImplementation* test_interface_or_null_member_cpp_value = V8TestInterface::ToImplWithTypeCheck(isolate, test_interface_or_null_member_value);
    if (!test_interface_or_null_member_cpp_value) {
      exception_state.ThrowTypeError("member testInterfaceOrNullMember is not of type TestInterface.");
      return;
    }
    impl->setTestInterfaceOrNullMember(test_interface_or_null_member_cpp_value);
  }

  v8::Local<v8::Value> test_interface_sequence_member_value;
  if (!v8Object->Get(context, keys[49].Get(isolate)).ToLocal(&test_interface_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (test_interface_sequence_member_value.IsEmpty() || test_interface_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<Member<TestInterfaceImplementation>> test_interface_sequence_member_cpp_value = NativeValueTraits<IDLSequence<TestInterfaceImplementation>>::NativeValue(isolate, test_interface_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setTestInterfaceSequenceMember(test_interface_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> test_object_sequence_member_value;
  if (!v8Object->Get(context, keys[50].Get(isolate)).ToLocal(&test_object_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (test_object_sequence_member_value.IsEmpty() || test_object_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<Member<TestObject>> test_object_sequence_member_cpp_value = NativeValueTraits<IDLSequence<TestObject>>::NativeValue(isolate, test_object_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setTestObjectSequenceMember(test_object_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> treat_non_null_obj_member_value;
  if (!v8Object->Get(context, keys[51].Get(isolate)).ToLocal(&treat_non_null_obj_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (treat_non_null_obj_member_value.IsEmpty() || treat_non_null_obj_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    if (!treat_non_null_obj_member_value->IsFunction()) {
      exception_state.ThrowTypeError("member treatNonNullObjMember is not a function.");
      return;
    }
    V8TreatNonObjectAsNullVoidFunction* treat_non_null_obj_member_cpp_value = V8TreatNonObjectAsNullVoidFunction::Create(treat_non_null_obj_member_value.As<v8::Function>());
    impl->setTreatNonNullObjMember(treat_non_null_obj_member_cpp_value);
  }

  v8::Local<v8::Value> treat_null_as_string_sequence_member_value;
  if (!v8Object->Get(context, keys[52].Get(isolate)).ToLocal(&treat_null_as_string_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (treat_null_as_string_sequence_member_value.IsEmpty() || treat_null_as_string_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    Vector<String> treat_null_as_string_sequence_member_cpp_value = NativeValueTraits<IDLSequence<IDLStringTreatNullAsEmptyString>>::NativeValue(isolate, treat_null_as_string_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setTreatNullAsStringSequenceMember(treat_null_as_string_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> uint8_array_member_value;
  if (!v8Object->Get(context, keys[53].Get(isolate)).ToLocal(&uint8_array_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (uint8_array_member_value.IsEmpty() || uint8_array_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    NotShared<DOMUint8Array> uint8_array_member_cpp_value = ToNotShared<NotShared<DOMUint8Array>>(isolate, uint8_array_member_value, exception_state);
    if (exception_state.HadException())
      return;
    if (!uint8_array_member_cpp_value) {
      exception_state.ThrowTypeError("member uint8ArrayMember is not of type Uint8Array.");
      return;
    }
    impl->setUint8ArrayMember(uint8_array_member_cpp_value);
  }

  v8::Local<v8::Value> union_in_record_member_value;
  if (!v8Object->Get(context, keys[54].Get(isolate)).ToLocal(&union_in_record_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (union_in_record_member_value.IsEmpty() || union_in_record_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<std::pair<String, LongOrBoolean>> union_in_record_member_cpp_value = NativeValueTraits<IDLRecord<IDLByteString, LongOrBoolean>>::NativeValue(isolate, union_in_record_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setUnionInRecordMember(union_in_record_member_cpp_value);
  }

  v8::Local<v8::Value> union_member_with_sequence_default_value;
  if (!v8Object->Get(context, keys[55].Get(isolate)).ToLocal(&union_member_with_sequence_default_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (union_member_with_sequence_default_value.IsEmpty() || union_member_with_sequence_default_value->IsUndefined()) {
    // Do nothing.
  } else {
    DoubleOrDoubleSequence union_member_with_sequence_default_cpp_value;
    V8DoubleOrDoubleSequence::ToImpl(isolate, union_member_with_sequence_default_value, union_member_with_sequence_default_cpp_value, UnionTypeConversionMode::kNotNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setUnionMemberWithSequenceDefault(union_member_with_sequence_default_cpp_value);
  }

  v8::Local<v8::Value> union_or_null_record_member_value;
  if (!v8Object->Get(context, keys[56].Get(isolate)).ToLocal(&union_or_null_record_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (union_or_null_record_member_value.IsEmpty() || union_or_null_record_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<std::pair<String, DoubleOrString>> union_or_null_record_member_cpp_value = NativeValueTraits<IDLRecord<IDLString, IDLNullable<DoubleOrString>>>::NativeValue(isolate, union_or_null_record_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setUnionOrNullRecordMember(union_or_null_record_member_cpp_value);
  }

  v8::Local<v8::Value> union_or_null_sequence_member_value;
  if (!v8Object->Get(context, keys[57].Get(isolate)).ToLocal(&union_or_null_sequence_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (union_or_null_sequence_member_value.IsEmpty() || union_or_null_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<DoubleOrString> union_or_null_sequence_member_cpp_value = NativeValueTraits<IDLSequence<IDLNullable<DoubleOrString>>>::NativeValue(isolate, union_or_null_sequence_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setUnionOrNullSequenceMember(union_or_null_sequence_member_cpp_value);
  }

  v8::Local<v8::Value> union_with_annotated_type_member_value;
  if (!v8Object->Get(context, keys[58].Get(isolate)).ToLocal(&union_with_annotated_type_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (union_with_annotated_type_member_value.IsEmpty() || union_with_annotated_type_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    StringTreatNullAsEmptyStringOrLong union_with_annotated_type_member_cpp_value;
    V8StringTreatNullAsEmptyStringOrLong::ToImpl(isolate, union_with_annotated_type_member_value, union_with_annotated_type_member_cpp_value, UnionTypeConversionMode::kNotNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setUnionWithAnnotatedTypeMember(union_with_annotated_type_member_cpp_value);
  }

  v8::Local<v8::Value> union_with_typedefs_value;
  if (!v8Object->Get(context, keys[59].Get(isolate)).ToLocal(&union_with_typedefs_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (union_with_typedefs_value.IsEmpty() || union_with_typedefs_value->IsUndefined()) {
    // Do nothing.
  } else {
    FloatOrBoolean union_with_typedefs_cpp_value;
    V8FloatOrBoolean::ToImpl(isolate, union_with_typedefs_value, union_with_typedefs_cpp_value, UnionTypeConversionMode::kNotNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setUnionWithTypedefs(union_with_typedefs_cpp_value);
  }

  v8::Local<v8::Value> unrestricted_double_member_value;
  if (!v8Object->Get(context, keys[60].Get(isolate)).ToLocal(&unrestricted_double_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (unrestricted_double_member_value.IsEmpty() || unrestricted_double_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    double unrestricted_double_member_cpp_value = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(isolate, unrestricted_double_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setUnrestrictedDoubleMember(unrestricted_double_member_cpp_value);
  }

  v8::Local<v8::Value> usv_string_or_null_member_value;
  if (!v8Object->Get(context, keys[61].Get(isolate)).ToLocal(&usv_string_or_null_member_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (usv_string_or_null_member_value.IsEmpty() || usv_string_or_null_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<kTreatNullAndUndefinedAsNullString> usv_string_or_null_member_cpp_value = NativeValueTraits<IDLUSVStringOrNull>::NativeValue(isolate, usv_string_or_null_member_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setUsvStringOrNullMember(usv_string_or_null_member_cpp_value);
  }

  if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
    v8::Local<v8::Value> runtime_member_value;
    if (!v8Object->Get(context, keys[36].Get(isolate)).ToLocal(&runtime_member_value)) {
      exception_state.RethrowV8Exception(block.Exception());
      return;
    }
    if (runtime_member_value.IsEmpty() || runtime_member_value->IsUndefined()) {
      // Do nothing.
    } else {
      bool runtime_member_cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(isolate, runtime_member_value, exception_state);
      if (exception_state.HadException())
        return;
      impl->setRuntimeMember(runtime_member_cpp_value);
    }

    v8::Local<v8::Value> runtime_second_member_value;
    if (!v8Object->Get(context, keys[37].Get(isolate)).ToLocal(&runtime_second_member_value)) {
      exception_state.RethrowV8Exception(block.Exception());
      return;
    }
    if (runtime_second_member_value.IsEmpty() || runtime_second_member_value->IsUndefined()) {
      // Do nothing.
    } else {
      bool runtime_second_member_cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(isolate, runtime_second_member_value, exception_state);
      if (exception_state.HadException())
        return;
      impl->setRuntimeSecondMember(runtime_second_member_cpp_value);
    }
  }

  if (RuntimeEnabledFeatures::FeatureNameEnabled(executionContext)) {
    v8::Local<v8::Value> origin_trial_member_value;
    if (!v8Object->Get(context, keys[29].Get(isolate)).ToLocal(&origin_trial_member_value)) {
      exception_state.RethrowV8Exception(block.Exception());
      return;
    }
    if (origin_trial_member_value.IsEmpty() || origin_trial_member_value->IsUndefined()) {
      // Do nothing.
    } else {
      bool origin_trial_member_cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(isolate, origin_trial_member_value, exception_state);
      if (exception_state.HadException())
        return;
      impl->setOriginTrialMember(origin_trial_member_cpp_value);
    }
  }

  if (RuntimeEnabledFeatures::FeatureName1Enabled(executionContext)) {
    v8::Local<v8::Value> origin_trial_second_member_value;
    if (!v8Object->Get(context, keys[30].Get(isolate)).ToLocal(&origin_trial_second_member_value)) {
      exception_state.RethrowV8Exception(block.Exception());
      return;
    }
    if (origin_trial_second_member_value.IsEmpty() || origin_trial_second_member_value->IsUndefined()) {
      // Do nothing.
    } else {
      bool origin_trial_second_member_cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(isolate, origin_trial_second_member_value, exception_state);
      if (exception_state.HadException())
        return;
      impl->setOriginTrialSecondMember(origin_trial_second_member_cpp_value);
    }
  }
}

v8::Local<v8::Value> TestDictionary::ToV8Impl(v8::Local<v8::Object> creationContext, v8::Isolate* isolate) const {
  v8::Local<v8::Object> v8Object = v8::Object::New(isolate);
  if (!toV8TestDictionary(this, v8Object, creationContext, isolate))
    return v8::Undefined(isolate);
  return v8Object;
}

bool toV8TestDictionary(const TestDictionary* impl, v8::Local<v8::Object> dictionary, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  const v8::Eternal<v8::Name>* keys = eternalV8TestDictionaryKeys(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  auto create_property = [dictionary, context, keys, isolate](
                             size_t key_index, v8::Local<v8::Value> value) {
    bool added_property;
    v8::Local<v8::Name> key = keys[key_index].Get(isolate);
    if (!dictionary->CreateDataProperty(context, key, value)
             .To(&added_property)) {
      return false;
    }
    return added_property;
  };

  ExecutionContext* executionContext = ToExecutionContext(context);
  DCHECK(executionContext);
  v8::Local<v8::Value> any_in_record_member_value;
  bool any_in_record_member_has_value_or_default = false;
  if (impl->hasAnyInRecordMember()) {
    any_in_record_member_value = ToV8(impl->anyInRecordMember(), creationContext, isolate);
    any_in_record_member_has_value_or_default = true;
  }
  if (any_in_record_member_has_value_or_default &&
      !create_property(0, any_in_record_member_value)) {
    return false;
  }

  v8::Local<v8::Value> any_member_value;
  bool any_member_has_value_or_default = false;
  if (impl->hasAnyMember()) {
    any_member_value = impl->anyMember().V8Value();
    any_member_has_value_or_default = true;
  } else {
    any_member_value = v8::Null(isolate);
    any_member_has_value_or_default = true;
  }
  if (any_member_has_value_or_default &&
      !create_property(1, any_member_value)) {
    return false;
  }

  v8::Local<v8::Value> applicable_to_type_long_member_value;
  bool applicable_to_type_long_member_has_value_or_default = false;
  if (impl->hasApplicableToTypeLongMember()) {
    applicable_to_type_long_member_value = v8::Integer::New(isolate, impl->applicableToTypeLongMember());
    applicable_to_type_long_member_has_value_or_default = true;
  }
  if (applicable_to_type_long_member_has_value_or_default &&
      !create_property(2, applicable_to_type_long_member_value)) {
    return false;
  }

  v8::Local<v8::Value> applicable_to_type_string_member_value;
  bool applicable_to_type_string_member_has_value_or_default = false;
  if (impl->hasApplicableToTypeStringMember()) {
    applicable_to_type_string_member_value = V8String(isolate, impl->applicableToTypeStringMember());
    applicable_to_type_string_member_has_value_or_default = true;
  }
  if (applicable_to_type_string_member_has_value_or_default &&
      !create_property(3, applicable_to_type_string_member_value)) {
    return false;
  }

  v8::Local<v8::Value> boolean_member_value;
  bool boolean_member_has_value_or_default = false;
  if (impl->hasBooleanMember()) {
    boolean_member_value = v8::Boolean::New(isolate, impl->booleanMember());
    boolean_member_has_value_or_default = true;
  }
  if (boolean_member_has_value_or_default &&
      !create_property(4, boolean_member_value)) {
    return false;
  }

  v8::Local<v8::Value> callback_function_member_value;
  bool callback_function_member_has_value_or_default = false;
  if (impl->hasCallbackFunctionMember()) {
    callback_function_member_value = ToV8(impl->callbackFunctionMember(), creationContext, isolate);
    callback_function_member_has_value_or_default = true;
  }
  if (callback_function_member_has_value_or_default &&
      !create_property(5, callback_function_member_value)) {
    return false;
  }

  v8::Local<v8::Value> create_value;
  bool create_has_value_or_default = false;
  if (impl->hasCreateMember()) {
    create_value = v8::Boolean::New(isolate, impl->createMember());
    create_has_value_or_default = true;
  }
  if (create_has_value_or_default &&
      !create_property(6, create_value)) {
    return false;
  }

  v8::Local<v8::Value> deprecated_create_member_value;
  bool deprecated_create_member_has_value_or_default = false;
  if (impl->hasCreateMember()) {
    deprecated_create_member_value = v8::Boolean::New(isolate, impl->createMember());
    deprecated_create_member_has_value_or_default = true;
  }
  if (deprecated_create_member_has_value_or_default &&
      !create_property(7, deprecated_create_member_value)) {
    return false;
  }

  v8::Local<v8::Value> dictionary_member_value;
  bool dictionary_member_has_value_or_default = false;
  if (impl->hasDictionaryMember()) {
    DCHECK(impl->dictionaryMember().IsObject());
    dictionary_member_value = impl->dictionaryMember().V8Value();
    dictionary_member_has_value_or_default = true;
  }
  if (dictionary_member_has_value_or_default &&
      !create_property(8, dictionary_member_value)) {
    return false;
  }

  v8::Local<v8::Value> dom_string_treat_null_as_empty_string_member_value;
  bool dom_string_treat_null_as_empty_string_member_has_value_or_default = false;
  if (impl->hasDomStringTreatNullAsEmptyStringMember()) {
    dom_string_treat_null_as_empty_string_member_value = V8String(isolate, impl->domStringTreatNullAsEmptyStringMember());
    dom_string_treat_null_as_empty_string_member_has_value_or_default = true;
  }
  if (dom_string_treat_null_as_empty_string_member_has_value_or_default &&
      !create_property(9, dom_string_treat_null_as_empty_string_member_value)) {
    return false;
  }

  v8::Local<v8::Value> double_or_null_member_value;
  bool double_or_null_member_has_value_or_default = false;
  if (impl->hasDoubleOrNullMember()) {
    double_or_null_member_value = v8::Number::New(isolate, impl->doubleOrNullMember());
    double_or_null_member_has_value_or_default = true;
  } else {
    double_or_null_member_value = v8::Null(isolate);
    double_or_null_member_has_value_or_default = true;
  }
  if (double_or_null_member_has_value_or_default &&
      !create_property(10, double_or_null_member_value)) {
    return false;
  }

  v8::Local<v8::Value> double_or_null_or_double_or_null_sequence_member_value;
  bool double_or_null_or_double_or_null_sequence_member_has_value_or_default = false;
  if (impl->hasDoubleOrNullOrDoubleOrNullSequenceMember()) {
    double_or_null_or_double_or_null_sequence_member_value = ToV8(impl->doubleOrNullOrDoubleOrNullSequenceMember(), creationContext, isolate);
    double_or_null_or_double_or_null_sequence_member_has_value_or_default = true;
  }
  if (double_or_null_or_double_or_null_sequence_member_has_value_or_default &&
      !create_property(11, double_or_null_or_double_or_null_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> double_or_null_record_member_value;
  bool double_or_null_record_member_has_value_or_default = false;
  if (impl->hasDoubleOrNullRecordMember()) {
    double_or_null_record_member_value = ToV8(impl->doubleOrNullRecordMember(), creationContext, isolate);
    double_or_null_record_member_has_value_or_default = true;
  }
  if (double_or_null_record_member_has_value_or_default &&
      !create_property(12, double_or_null_record_member_value)) {
    return false;
  }

  v8::Local<v8::Value> double_or_null_sequence_member_value;
  bool double_or_null_sequence_member_has_value_or_default = false;
  if (impl->hasDoubleOrNullSequenceMember()) {
    double_or_null_sequence_member_value = ToV8(impl->doubleOrNullSequenceMember(), creationContext, isolate);
    double_or_null_sequence_member_has_value_or_default = true;
  }
  if (double_or_null_sequence_member_has_value_or_default &&
      !create_property(13, double_or_null_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> double_or_string_member_value;
  bool double_or_string_member_has_value_or_default = false;
  if (impl->hasDoubleOrStringMember()) {
    double_or_string_member_value = ToV8(impl->doubleOrStringMember(), creationContext, isolate);
    double_or_string_member_has_value_or_default = true;
  } else {
    double_or_string_member_value = ToV8(DoubleOrString::FromDouble(3.14), creationContext, isolate);
    double_or_string_member_has_value_or_default = true;
  }
  if (double_or_string_member_has_value_or_default &&
      !create_property(14, double_or_string_member_value)) {
    return false;
  }

  v8::Local<v8::Value> double_or_string_sequence_member_value;
  bool double_or_string_sequence_member_has_value_or_default = false;
  if (impl->hasDoubleOrStringSequenceMember()) {
    double_or_string_sequence_member_value = ToV8(impl->doubleOrStringSequenceMember(), creationContext, isolate);
    double_or_string_sequence_member_has_value_or_default = true;
  }
  if (double_or_string_sequence_member_has_value_or_default &&
      !create_property(15, double_or_string_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> element_or_null_member_value;
  bool element_or_null_member_has_value_or_default = false;
  if (impl->hasElementOrNullMember()) {
    element_or_null_member_value = ToV8(impl->elementOrNullMember(), creationContext, isolate);
    element_or_null_member_has_value_or_default = true;
  }
  if (element_or_null_member_has_value_or_default &&
      !create_property(16, element_or_null_member_value)) {
    return false;
  }

  v8::Local<v8::Value> element_or_null_record_member_value;
  bool element_or_null_record_member_has_value_or_default = false;
  if (impl->hasElementOrNullRecordMember()) {
    element_or_null_record_member_value = ToV8(impl->elementOrNullRecordMember(), creationContext, isolate);
    element_or_null_record_member_has_value_or_default = true;
  }
  if (element_or_null_record_member_has_value_or_default &&
      !create_property(17, element_or_null_record_member_value)) {
    return false;
  }

  v8::Local<v8::Value> element_or_null_sequence_member_value;
  bool element_or_null_sequence_member_has_value_or_default = false;
  if (impl->hasElementOrNullSequenceMember()) {
    element_or_null_sequence_member_value = ToV8(impl->elementOrNullSequenceMember(), creationContext, isolate);
    element_or_null_sequence_member_has_value_or_default = true;
  }
  if (element_or_null_sequence_member_has_value_or_default &&
      !create_property(18, element_or_null_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> enum_member_value;
  bool enum_member_has_value_or_default = false;
  if (impl->hasEnumMember()) {
    enum_member_value = V8String(isolate, impl->enumMember());
    enum_member_has_value_or_default = true;
  } else {
    enum_member_value = V8String(isolate, "foo");
    enum_member_has_value_or_default = true;
  }
  if (enum_member_has_value_or_default &&
      !create_property(19, enum_member_value)) {
    return false;
  }

  v8::Local<v8::Value> enum_or_null_member_value;
  bool enum_or_null_member_has_value_or_default = false;
  if (impl->hasEnumOrNullMember()) {
    enum_or_null_member_value = V8String(isolate, impl->enumOrNullMember());
    enum_or_null_member_has_value_or_default = true;
  } else {
    enum_or_null_member_value = v8::Null(isolate);
    enum_or_null_member_has_value_or_default = true;
  }
  if (enum_or_null_member_has_value_or_default &&
      !create_property(20, enum_or_null_member_value)) {
    return false;
  }

  v8::Local<v8::Value> enum_sequence_member_value;
  bool enum_sequence_member_has_value_or_default = false;
  if (impl->hasEnumSequenceMember()) {
    enum_sequence_member_value = ToV8(impl->enumSequenceMember(), creationContext, isolate);
    enum_sequence_member_has_value_or_default = true;
  }
  if (enum_sequence_member_has_value_or_default &&
      !create_property(21, enum_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> event_target_member_value;
  bool event_target_member_has_value_or_default = false;
  if (impl->hasEventTargetMember()) {
    event_target_member_value = ToV8(impl->eventTargetMember(), creationContext, isolate);
    event_target_member_has_value_or_default = true;
  }
  if (event_target_member_has_value_or_default &&
      !create_property(22, event_target_member_value)) {
    return false;
  }

  v8::Local<v8::Value> garbage_collected_record_member_value;
  bool garbage_collected_record_member_has_value_or_default = false;
  if (impl->hasGarbageCollectedRecordMember()) {
    garbage_collected_record_member_value = ToV8(impl->garbageCollectedRecordMember(), creationContext, isolate);
    garbage_collected_record_member_has_value_or_default = true;
  }
  if (garbage_collected_record_member_has_value_or_default &&
      !create_property(23, garbage_collected_record_member_value)) {
    return false;
  }

  v8::Local<v8::Value> internal_dictionary_sequence_member_value;
  bool internal_dictionary_sequence_member_has_value_or_default = false;
  if (impl->hasInternalDictionarySequenceMember()) {
    internal_dictionary_sequence_member_value = ToV8(impl->internalDictionarySequenceMember(), creationContext, isolate);
    internal_dictionary_sequence_member_has_value_or_default = true;
  }
  if (internal_dictionary_sequence_member_has_value_or_default &&
      !create_property(24, internal_dictionary_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> long_member_value;
  bool long_member_has_value_or_default = false;
  if (impl->hasLongMember()) {
    long_member_value = v8::Integer::New(isolate, impl->longMember());
    long_member_has_value_or_default = true;
  } else {
    long_member_value = v8::Integer::New(isolate, 1);
    long_member_has_value_or_default = true;
  }
  if (long_member_has_value_or_default &&
      !create_property(25, long_member_value)) {
    return false;
  }

  v8::Local<v8::Value> member_with_hyphen_in_name_value;
  bool member_with_hyphen_in_name_has_value_or_default = false;
  if (impl->hasMemberWithHyphenInName()) {
    member_with_hyphen_in_name_value = v8::Boolean::New(isolate, impl->memberWithHyphenInName());
    member_with_hyphen_in_name_has_value_or_default = true;
  } else {
    member_with_hyphen_in_name_value = v8::Boolean::New(isolate, false);
    member_with_hyphen_in_name_has_value_or_default = true;
  }
  if (member_with_hyphen_in_name_has_value_or_default &&
      !create_property(26, member_with_hyphen_in_name_value)) {
    return false;
  }

  v8::Local<v8::Value> object_member_value;
  bool object_member_has_value_or_default = false;
  if (impl->hasObjectMember()) {
    DCHECK(impl->objectMember().IsObject());
    object_member_value = impl->objectMember().V8Value();
    object_member_has_value_or_default = true;
  }
  if (object_member_has_value_or_default &&
      !create_property(27, object_member_value)) {
    return false;
  }

  v8::Local<v8::Value> object_or_null_member_value;
  bool object_or_null_member_has_value_or_default = false;
  if (impl->hasObjectOrNullMember()) {
    DCHECK(impl->objectOrNullMember().IsObject());
    object_or_null_member_value = impl->objectOrNullMember().V8Value();
    object_or_null_member_has_value_or_default = true;
  } else {
    object_or_null_member_value = v8::Null(isolate);
    object_or_null_member_has_value_or_default = true;
  }
  if (object_or_null_member_has_value_or_default &&
      !create_property(28, object_or_null_member_value)) {
    return false;
  }

  v8::Local<v8::Value> other_double_or_string_member_value;
  bool other_double_or_string_member_has_value_or_default = false;
  if (impl->hasOtherDoubleOrStringMember()) {
    other_double_or_string_member_value = ToV8(impl->otherDoubleOrStringMember(), creationContext, isolate);
    other_double_or_string_member_has_value_or_default = true;
  } else {
    other_double_or_string_member_value = ToV8(DoubleOrString::FromString("default string value"), creationContext, isolate);
    other_double_or_string_member_has_value_or_default = true;
  }
  if (other_double_or_string_member_has_value_or_default &&
      !create_property(31, other_double_or_string_member_value)) {
    return false;
  }

  v8::Local<v8::Value> public_value;
  bool public_has_value_or_default = false;
  if (impl->hasIsPublic()) {
    public_value = v8::Boolean::New(isolate, impl->isPublic());
    public_has_value_or_default = true;
  }
  if (public_has_value_or_default &&
      !create_property(32, public_value)) {
    return false;
  }

  v8::Local<v8::Value> record_member_value;
  bool record_member_has_value_or_default = false;
  if (impl->hasRecordMember()) {
    record_member_value = ToV8(impl->recordMember(), creationContext, isolate);
    record_member_has_value_or_default = true;
  }
  if (record_member_has_value_or_default &&
      !create_property(33, record_member_value)) {
    return false;
  }

  v8::Local<v8::Value> required_callback_function_member_value;
  bool required_callback_function_member_has_value_or_default = false;
  if (impl->hasRequiredCallbackFunctionMember()) {
    required_callback_function_member_value = ToV8(impl->requiredCallbackFunctionMember(), creationContext, isolate);
    required_callback_function_member_has_value_or_default = true;
  } else {
    NOTREACHED();
  }
  if (required_callback_function_member_has_value_or_default &&
      !create_property(34, required_callback_function_member_value)) {
    return false;
  }

  v8::Local<v8::Value> restricted_double_member_value;
  bool restricted_double_member_has_value_or_default = false;
  if (impl->hasRestrictedDoubleMember()) {
    restricted_double_member_value = v8::Number::New(isolate, impl->restrictedDoubleMember());
    restricted_double_member_has_value_or_default = true;
  } else {
    restricted_double_member_value = v8::Number::New(isolate, 3.14);
    restricted_double_member_has_value_or_default = true;
  }
  if (restricted_double_member_has_value_or_default &&
      !create_property(35, restricted_double_member_value)) {
    return false;
  }

  v8::Local<v8::Value> string_member_value;
  bool string_member_has_value_or_default = false;
  if (impl->hasStringMember()) {
    string_member_value = V8String(isolate, impl->stringMember());
    string_member_has_value_or_default = true;
  }
  if (string_member_has_value_or_default &&
      !create_property(38, string_member_value)) {
    return false;
  }

  v8::Local<v8::Value> string_or_null_member_value;
  bool string_or_null_member_has_value_or_default = false;
  if (impl->hasStringOrNullMember()) {
    string_or_null_member_value = V8String(isolate, impl->stringOrNullMember());
    string_or_null_member_has_value_or_default = true;
  } else {
    string_or_null_member_value = V8String(isolate, "default string value");
    string_or_null_member_has_value_or_default = true;
  }
  if (string_or_null_member_has_value_or_default &&
      !create_property(39, string_or_null_member_value)) {
    return false;
  }

  v8::Local<v8::Value> string_or_null_record_member_value;
  bool string_or_null_record_member_has_value_or_default = false;
  if (impl->hasStringOrNullRecordMember()) {
    string_or_null_record_member_value = ToV8(impl->stringOrNullRecordMember(), creationContext, isolate);
    string_or_null_record_member_has_value_or_default = true;
  }
  if (string_or_null_record_member_has_value_or_default &&
      !create_property(40, string_or_null_record_member_value)) {
    return false;
  }

  v8::Local<v8::Value> string_or_null_sequence_member_value;
  bool string_or_null_sequence_member_has_value_or_default = false;
  if (impl->hasStringOrNullSequenceMember()) {
    string_or_null_sequence_member_value = ToV8(impl->stringOrNullSequenceMember(), creationContext, isolate);
    string_or_null_sequence_member_has_value_or_default = true;
  }
  if (string_or_null_sequence_member_has_value_or_default &&
      !create_property(41, string_or_null_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> string_sequence_member_value;
  bool string_sequence_member_has_value_or_default = false;
  if (impl->hasStringSequenceMember()) {
    string_sequence_member_value = ToV8(impl->stringSequenceMember(), creationContext, isolate);
    string_sequence_member_has_value_or_default = true;
  } else {
    string_sequence_member_value = ToV8(Vector<String>(), creationContext, isolate);
    string_sequence_member_has_value_or_default = true;
  }
  if (string_sequence_member_has_value_or_default &&
      !create_property(42, string_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> test_enum_or_null_or_test_enum_sequence_member_value;
  bool test_enum_or_null_or_test_enum_sequence_member_has_value_or_default = false;
  if (impl->hasTestEnumOrNullOrTestEnumSequenceMember()) {
    test_enum_or_null_or_test_enum_sequence_member_value = ToV8(impl->testEnumOrNullOrTestEnumSequenceMember(), creationContext, isolate);
    test_enum_or_null_or_test_enum_sequence_member_has_value_or_default = true;
  }
  if (test_enum_or_null_or_test_enum_sequence_member_has_value_or_default &&
      !create_property(43, test_enum_or_null_or_test_enum_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> test_enum_or_test_enum_or_null_sequence_member_value;
  bool test_enum_or_test_enum_or_null_sequence_member_has_value_or_default = false;
  if (impl->hasTestEnumOrTestEnumOrNullSequenceMember()) {
    test_enum_or_test_enum_or_null_sequence_member_value = ToV8(impl->testEnumOrTestEnumOrNullSequenceMember(), creationContext, isolate);
    test_enum_or_test_enum_or_null_sequence_member_has_value_or_default = true;
  }
  if (test_enum_or_test_enum_or_null_sequence_member_has_value_or_default &&
      !create_property(44, test_enum_or_test_enum_or_null_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> test_enum_or_test_enum_sequence_member_value;
  bool test_enum_or_test_enum_sequence_member_has_value_or_default = false;
  if (impl->hasTestEnumOrTestEnumSequenceMember()) {
    test_enum_or_test_enum_sequence_member_value = ToV8(impl->testEnumOrTestEnumSequenceMember(), creationContext, isolate);
    test_enum_or_test_enum_sequence_member_has_value_or_default = true;
  }
  if (test_enum_or_test_enum_sequence_member_has_value_or_default &&
      !create_property(45, test_enum_or_test_enum_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> test_interface_2_or_uint8_array_member_value;
  bool test_interface_2_or_uint8_array_member_has_value_or_default = false;
  if (impl->hasTestInterface2OrUint8ArrayMember()) {
    test_interface_2_or_uint8_array_member_value = ToV8(impl->testInterface2OrUint8ArrayMember(), creationContext, isolate);
    test_interface_2_or_uint8_array_member_has_value_or_default = true;
  }
  if (test_interface_2_or_uint8_array_member_has_value_or_default &&
      !create_property(46, test_interface_2_or_uint8_array_member_value)) {
    return false;
  }

  v8::Local<v8::Value> test_interface_member_value;
  bool test_interface_member_has_value_or_default = false;
  if (impl->hasTestInterfaceMember()) {
    test_interface_member_value = ToV8(impl->testInterfaceMember(), creationContext, isolate);
    test_interface_member_has_value_or_default = true;
  }
  if (test_interface_member_has_value_or_default &&
      !create_property(47, test_interface_member_value)) {
    return false;
  }

  v8::Local<v8::Value> test_interface_or_null_member_value;
  bool test_interface_or_null_member_has_value_or_default = false;
  if (impl->hasTestInterfaceOrNullMember()) {
    test_interface_or_null_member_value = ToV8(impl->testInterfaceOrNullMember(), creationContext, isolate);
    test_interface_or_null_member_has_value_or_default = true;
  }
  if (test_interface_or_null_member_has_value_or_default &&
      !create_property(48, test_interface_or_null_member_value)) {
    return false;
  }

  v8::Local<v8::Value> test_interface_sequence_member_value;
  bool test_interface_sequence_member_has_value_or_default = false;
  if (impl->hasTestInterfaceSequenceMember()) {
    test_interface_sequence_member_value = ToV8(impl->testInterfaceSequenceMember(), creationContext, isolate);
    test_interface_sequence_member_has_value_or_default = true;
  } else {
    test_interface_sequence_member_value = ToV8(HeapVector<Member<TestInterfaceImplementation>>(), creationContext, isolate);
    test_interface_sequence_member_has_value_or_default = true;
  }
  if (test_interface_sequence_member_has_value_or_default &&
      !create_property(49, test_interface_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> test_object_sequence_member_value;
  bool test_object_sequence_member_has_value_or_default = false;
  if (impl->hasTestObjectSequenceMember()) {
    test_object_sequence_member_value = ToV8(impl->testObjectSequenceMember(), creationContext, isolate);
    test_object_sequence_member_has_value_or_default = true;
  }
  if (test_object_sequence_member_has_value_or_default &&
      !create_property(50, test_object_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> treat_non_null_obj_member_value;
  bool treat_non_null_obj_member_has_value_or_default = false;
  if (impl->hasTreatNonNullObjMember()) {
    treat_non_null_obj_member_value = ToV8(impl->treatNonNullObjMember(), creationContext, isolate);
    treat_non_null_obj_member_has_value_or_default = true;
  }
  if (treat_non_null_obj_member_has_value_or_default &&
      !create_property(51, treat_non_null_obj_member_value)) {
    return false;
  }

  v8::Local<v8::Value> treat_null_as_string_sequence_member_value;
  bool treat_null_as_string_sequence_member_has_value_or_default = false;
  if (impl->hasTreatNullAsStringSequenceMember()) {
    treat_null_as_string_sequence_member_value = ToV8(impl->treatNullAsStringSequenceMember(), creationContext, isolate);
    treat_null_as_string_sequence_member_has_value_or_default = true;
  } else {
    treat_null_as_string_sequence_member_value = ToV8(Vector<String>(), creationContext, isolate);
    treat_null_as_string_sequence_member_has_value_or_default = true;
  }
  if (treat_null_as_string_sequence_member_has_value_or_default &&
      !create_property(52, treat_null_as_string_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> uint8_array_member_value;
  bool uint8_array_member_has_value_or_default = false;
  if (impl->hasUint8ArrayMember()) {
    uint8_array_member_value = ToV8(impl->uint8ArrayMember(), creationContext, isolate);
    uint8_array_member_has_value_or_default = true;
  }
  if (uint8_array_member_has_value_or_default &&
      !create_property(53, uint8_array_member_value)) {
    return false;
  }

  v8::Local<v8::Value> union_in_record_member_value;
  bool union_in_record_member_has_value_or_default = false;
  if (impl->hasUnionInRecordMember()) {
    union_in_record_member_value = ToV8(impl->unionInRecordMember(), creationContext, isolate);
    union_in_record_member_has_value_or_default = true;
  }
  if (union_in_record_member_has_value_or_default &&
      !create_property(54, union_in_record_member_value)) {
    return false;
  }

  v8::Local<v8::Value> union_member_with_sequence_default_value;
  bool union_member_with_sequence_default_has_value_or_default = false;
  if (impl->hasUnionMemberWithSequenceDefault()) {
    union_member_with_sequence_default_value = ToV8(impl->unionMemberWithSequenceDefault(), creationContext, isolate);
    union_member_with_sequence_default_has_value_or_default = true;
  } else {
    union_member_with_sequence_default_value = ToV8(DoubleOrDoubleSequence::FromDoubleSequence(Vector<double>()), creationContext, isolate);
    union_member_with_sequence_default_has_value_or_default = true;
  }
  if (union_member_with_sequence_default_has_value_or_default &&
      !create_property(55, union_member_with_sequence_default_value)) {
    return false;
  }

  v8::Local<v8::Value> union_or_null_record_member_value;
  bool union_or_null_record_member_has_value_or_default = false;
  if (impl->hasUnionOrNullRecordMember()) {
    union_or_null_record_member_value = ToV8(impl->unionOrNullRecordMember(), creationContext, isolate);
    union_or_null_record_member_has_value_or_default = true;
  }
  if (union_or_null_record_member_has_value_or_default &&
      !create_property(56, union_or_null_record_member_value)) {
    return false;
  }

  v8::Local<v8::Value> union_or_null_sequence_member_value;
  bool union_or_null_sequence_member_has_value_or_default = false;
  if (impl->hasUnionOrNullSequenceMember()) {
    union_or_null_sequence_member_value = ToV8(impl->unionOrNullSequenceMember(), creationContext, isolate);
    union_or_null_sequence_member_has_value_or_default = true;
  }
  if (union_or_null_sequence_member_has_value_or_default &&
      !create_property(57, union_or_null_sequence_member_value)) {
    return false;
  }

  v8::Local<v8::Value> union_with_annotated_type_member_value;
  bool union_with_annotated_type_member_has_value_or_default = false;
  if (impl->hasUnionWithAnnotatedTypeMember()) {
    union_with_annotated_type_member_value = ToV8(impl->unionWithAnnotatedTypeMember(), creationContext, isolate);
    union_with_annotated_type_member_has_value_or_default = true;
  }
  if (union_with_annotated_type_member_has_value_or_default &&
      !create_property(58, union_with_annotated_type_member_value)) {
    return false;
  }

  v8::Local<v8::Value> union_with_typedefs_value;
  bool union_with_typedefs_has_value_or_default = false;
  if (impl->hasUnionWithTypedefs()) {
    union_with_typedefs_value = ToV8(impl->unionWithTypedefs(), creationContext, isolate);
    union_with_typedefs_has_value_or_default = true;
  }
  if (union_with_typedefs_has_value_or_default &&
      !create_property(59, union_with_typedefs_value)) {
    return false;
  }

  v8::Local<v8::Value> unrestricted_double_member_value;
  bool unrestricted_double_member_has_value_or_default = false;
  if (impl->hasUnrestrictedDoubleMember()) {
    unrestricted_double_member_value = v8::Number::New(isolate, impl->unrestrictedDoubleMember());
    unrestricted_double_member_has_value_or_default = true;
  } else {
    unrestricted_double_member_value = v8::Number::New(isolate, 3.14);
    unrestricted_double_member_has_value_or_default = true;
  }
  if (unrestricted_double_member_has_value_or_default &&
      !create_property(60, unrestricted_double_member_value)) {
    return false;
  }

  v8::Local<v8::Value> usv_string_or_null_member_value;
  bool usv_string_or_null_member_has_value_or_default = false;
  if (impl->hasUsvStringOrNullMember()) {
    usv_string_or_null_member_value = V8String(isolate, impl->usvStringOrNullMember());
    usv_string_or_null_member_has_value_or_default = true;
  } else {
    usv_string_or_null_member_value = v8::Null(isolate);
    usv_string_or_null_member_has_value_or_default = true;
  }
  if (usv_string_or_null_member_has_value_or_default &&
      !create_property(61, usv_string_or_null_member_value)) {
    return false;
  }

  if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
    v8::Local<v8::Value> runtime_member_value;
    bool runtime_member_has_value_or_default = false;
    if (impl->hasRuntimeMember()) {
      runtime_member_value = v8::Boolean::New(isolate, impl->runtimeMember());
      runtime_member_has_value_or_default = true;
    }
    if (runtime_member_has_value_or_default &&
        !create_property(36, runtime_member_value)) {
      return false;
    }

    v8::Local<v8::Value> runtime_second_member_value;
    bool runtime_second_member_has_value_or_default = false;
    if (impl->hasRuntimeSecondMember()) {
      runtime_second_member_value = v8::Boolean::New(isolate, impl->runtimeSecondMember());
      runtime_second_member_has_value_or_default = true;
    }
    if (runtime_second_member_has_value_or_default &&
        !create_property(37, runtime_second_member_value)) {
      return false;
    }
  }

  if (RuntimeEnabledFeatures::FeatureNameEnabled(executionContext)) {
    v8::Local<v8::Value> origin_trial_member_value;
    bool origin_trial_member_has_value_or_default = false;
    if (impl->hasOriginTrialMember()) {
      origin_trial_member_value = v8::Boolean::New(isolate, impl->originTrialMember());
      origin_trial_member_has_value_or_default = true;
    }
    if (origin_trial_member_has_value_or_default &&
        !create_property(29, origin_trial_member_value)) {
      return false;
    }
  }

  if (RuntimeEnabledFeatures::FeatureName1Enabled(executionContext)) {
    v8::Local<v8::Value> origin_trial_second_member_value;
    bool origin_trial_second_member_has_value_or_default = false;
    if (impl->hasOriginTrialSecondMember()) {
      origin_trial_second_member_value = v8::Boolean::New(isolate, impl->originTrialSecondMember());
      origin_trial_second_member_has_value_or_default = true;
    }
    if (origin_trial_second_member_has_value_or_default &&
        !create_property(30, origin_trial_second_member_value)) {
      return false;
    }
  }

  return true;
}

TestDictionary* NativeValueTraits<TestDictionary>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestDictionary* impl = TestDictionary::Create();
  V8TestDictionary::ToImpl(isolate, value, impl, exception_state);
  return impl;
}

}  // namespace blink
