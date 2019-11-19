// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_impl.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_DICTIONARY_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_DICTIONARY_H_

#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_double_or_null_sequence.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_double_sequence.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_string.h"
#include "third_party/blink/renderer/bindings/core/v8/float_or_boolean.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_dictionary_base.h"
#include "third_party/blink/renderer/bindings/core/v8/long_or_boolean.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/string_treat_null_as_empty_string_or_long.h"
#include "third_party/blink/renderer/bindings/core/v8/test_enum_or_test_enum_or_null_sequence.h"
#include "third_party/blink/renderer/bindings/core/v8/test_enum_or_test_enum_sequence.h"
#include "third_party/blink/renderer/bindings/core/v8/test_interface_2_or_uint8_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_treat_non_object_as_null_void_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_callback_function.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_2.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/testing/internal_dictionary.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class EventTarget;
class TestObject;
class TestInterfaceImplementation;
class Element;

class CORE_EXPORT TestDictionary : public IDLDictionaryBase {
 public:
  static TestDictionary* Create() { return MakeGarbageCollected<TestDictionary>(); }

  TestDictionary();
  virtual ~TestDictionary();

  bool hasAnyInRecordMember() const { return has_any_in_record_member_; }
  const HeapVector<std::pair<String, ScriptValue>>& anyInRecordMember() const {
    DCHECK(has_any_in_record_member_);
    return any_in_record_member_;
  }
  void setAnyInRecordMember(const HeapVector<std::pair<String, ScriptValue>>&);

  bool hasAnyMember() const { return !(any_member_.IsEmpty() || any_member_.IsUndefined()); }
  ScriptValue anyMember() const {
    return any_member_;
  }
  void setAnyMember(ScriptValue);

  bool hasApplicableToTypeLongMember() const { return has_applicable_to_type_long_member_; }
  int32_t applicableToTypeLongMember() const {
    DCHECK(has_applicable_to_type_long_member_);
    return applicable_to_type_long_member_;
  }
  inline void setApplicableToTypeLongMember(int32_t);

  bool hasApplicableToTypeStringMember() const { return !applicable_to_type_string_member_.IsNull(); }
  const String& applicableToTypeStringMember() const {
    return applicable_to_type_string_member_;
  }
  inline void setApplicableToTypeStringMember(const String&);

  bool hasBooleanMember() const { return has_boolean_member_; }
  bool booleanMember() const {
    DCHECK(has_boolean_member_);
    return boolean_member_;
  }
  inline void setBooleanMember(bool);

  bool hasCallbackFunctionMember() const { return callback_function_member_; }
  V8VoidCallbackFunction* callbackFunctionMember() const {
    return callback_function_member_;
  }
  void setCallbackFunctionMember(V8VoidCallbackFunction*);

  bool hasCreateMember() const { return has_create_member_; }
  bool createMember() const {
    DCHECK(has_create_member_);
    return create_member_;
  }
  inline void setCreateMember(bool);

  bool hasDictionaryMember() const { return !dictionary_member_.IsUndefinedOrNull(); }
  Dictionary dictionaryMember() const {
    return dictionary_member_;
  }
  void setDictionaryMember(Dictionary);

  bool hasDomStringTreatNullAsEmptyStringMember() const { return !dom_string_treat_null_as_empty_string_member_.IsNull(); }
  const String& domStringTreatNullAsEmptyStringMember() const {
    return dom_string_treat_null_as_empty_string_member_;
  }
  inline void setDomStringTreatNullAsEmptyStringMember(const String&);

  bool hasDoubleOrNullMember() const { return has_double_or_null_member_; }
  double doubleOrNullMember() const {
    DCHECK(has_double_or_null_member_);
    return double_or_null_member_;
  }
  inline void setDoubleOrNullMember(double);
  inline void setDoubleOrNullMemberToNull();

  bool hasDoubleOrNullOrDoubleOrNullSequenceMember() const { return !double_or_null_or_double_or_null_sequence_member_.IsNull(); }
  const DoubleOrDoubleOrNullSequence& doubleOrNullOrDoubleOrNullSequenceMember() const {
    return double_or_null_or_double_or_null_sequence_member_;
  }
  void setDoubleOrNullOrDoubleOrNullSequenceMember(const DoubleOrDoubleOrNullSequence&);

  bool hasDoubleOrNullRecordMember() const { return has_double_or_null_record_member_; }
  const Vector<std::pair<String, base::Optional<double>>>& doubleOrNullRecordMember() const {
    DCHECK(has_double_or_null_record_member_);
    return double_or_null_record_member_;
  }
  void setDoubleOrNullRecordMember(const Vector<std::pair<String, base::Optional<double>>>&);

  bool hasDoubleOrNullSequenceMember() const { return has_double_or_null_sequence_member_; }
  const Vector<base::Optional<double>>& doubleOrNullSequenceMember() const {
    DCHECK(has_double_or_null_sequence_member_);
    return double_or_null_sequence_member_;
  }
  void setDoubleOrNullSequenceMember(const Vector<base::Optional<double>>&);

  bool hasDoubleOrStringMember() const { return !double_or_string_member_.IsNull(); }
  const DoubleOrString& doubleOrStringMember() const {
    return double_or_string_member_;
  }
  void setDoubleOrStringMember(const DoubleOrString&);

  bool hasDoubleOrStringSequenceMember() const { return has_double_or_string_sequence_member_; }
  const HeapVector<DoubleOrString>& doubleOrStringSequenceMember() const {
    DCHECK(has_double_or_string_sequence_member_);
    return double_or_string_sequence_member_;
  }
  void setDoubleOrStringSequenceMember(const HeapVector<DoubleOrString>&);

  bool hasElementOrNullMember() const { return has_element_or_null_member_; }
  Element* elementOrNullMember() const {
    return element_or_null_member_;
  }
  inline void setElementOrNullMember(Element*);
  inline void setElementOrNullMemberToNull();

  bool hasElementOrNullRecordMember() const { return has_element_or_null_record_member_; }
  const HeapVector<std::pair<String, Member<Element>>>& elementOrNullRecordMember() const {
    DCHECK(has_element_or_null_record_member_);
    return element_or_null_record_member_;
  }
  void setElementOrNullRecordMember(const HeapVector<std::pair<String, Member<Element>>>&);

  bool hasElementOrNullSequenceMember() const { return has_element_or_null_sequence_member_; }
  const HeapVector<Member<Element>>& elementOrNullSequenceMember() const {
    DCHECK(has_element_or_null_sequence_member_);
    return element_or_null_sequence_member_;
  }
  void setElementOrNullSequenceMember(const HeapVector<Member<Element>>&);

  bool hasEnumMember() const { return !enum_member_.IsNull(); }
  const String& enumMember() const {
    return enum_member_;
  }
  inline void setEnumMember(const String&);

  bool hasEnumOrNullMember() const { return !enum_or_null_member_.IsNull(); }
  const String& enumOrNullMember() const {
    return enum_or_null_member_;
  }
  inline void setEnumOrNullMember(const String&);
  inline void setEnumOrNullMemberToNull();

  bool hasEnumSequenceMember() const { return has_enum_sequence_member_; }
  const Vector<String>& enumSequenceMember() const {
    DCHECK(has_enum_sequence_member_);
    return enum_sequence_member_;
  }
  void setEnumSequenceMember(const Vector<String>&);

  bool hasEventTargetMember() const { return event_target_member_; }
  EventTarget* eventTargetMember() const {
    return event_target_member_;
  }
  inline void setEventTargetMember(EventTarget*);

  bool hasGarbageCollectedRecordMember() const { return has_garbage_collected_record_member_; }
  const HeapVector<std::pair<String, Member<TestObject>>>& garbageCollectedRecordMember() const {
    DCHECK(has_garbage_collected_record_member_);
    return garbage_collected_record_member_;
  }
  void setGarbageCollectedRecordMember(const HeapVector<std::pair<String, Member<TestObject>>>&);

  bool hasInternalDictionarySequenceMember() const { return has_internal_dictionary_sequence_member_; }
  const HeapVector<Member<InternalDictionary>>& internalDictionarySequenceMember() const {
    DCHECK(has_internal_dictionary_sequence_member_);
    return internal_dictionary_sequence_member_;
  }
  void setInternalDictionarySequenceMember(const HeapVector<Member<InternalDictionary>>&);

  bool hasIsPublic() const { return has_is_public_; }
  bool isPublic() const {
    DCHECK(has_is_public_);
    return is_public_;
  }
  inline void setIsPublic(bool);

  bool hasLongMember() const { return has_long_member_; }
  int32_t longMember() const {
    DCHECK(has_long_member_);
    return long_member_;
  }
  inline void setLongMember(int32_t);

  bool hasMemberWithHyphenInName() const { return has_member_with_hyphen_in_name_; }
  bool memberWithHyphenInName() const {
    DCHECK(has_member_with_hyphen_in_name_);
    return member_with_hyphen_in_name_;
  }
  inline void setMemberWithHyphenInName(bool);

  bool hasObjectMember() const { return !(object_member_.IsEmpty() || object_member_.IsNull() || object_member_.IsUndefined()); }
  ScriptValue objectMember() const {
    return object_member_;
  }
  void setObjectMember(ScriptValue);

  bool hasObjectOrNullMember() const { return !(object_or_null_member_.IsEmpty() || object_or_null_member_.IsNull() || object_or_null_member_.IsUndefined()); }
  ScriptValue objectOrNullMember() const {
    return object_or_null_member_;
  }
  void setObjectOrNullMember(ScriptValue);
  void setObjectOrNullMemberToNull();

  bool hasOriginTrialMember() const { return has_origin_trial_member_; }
  bool originTrialMember() const {
    DCHECK(has_origin_trial_member_);
    return origin_trial_member_;
  }
  inline void setOriginTrialMember(bool);

  bool hasOriginTrialSecondMember() const { return has_origin_trial_second_member_; }
  bool originTrialSecondMember() const {
    DCHECK(has_origin_trial_second_member_);
    return origin_trial_second_member_;
  }
  inline void setOriginTrialSecondMember(bool);

  bool hasOtherDoubleOrStringMember() const { return !other_double_or_string_member_.IsNull(); }
  const DoubleOrString& otherDoubleOrStringMember() const {
    return other_double_or_string_member_;
  }
  void setOtherDoubleOrStringMember(const DoubleOrString&);

  bool hasRecordMember() const { return has_record_member_; }
  const Vector<std::pair<String, int8_t>>& recordMember() const {
    DCHECK(has_record_member_);
    return record_member_;
  }
  void setRecordMember(const Vector<std::pair<String, int8_t>>&);

  bool hasRequiredCallbackFunctionMember() const { return required_callback_function_member_; }
  V8VoidCallbackFunction* requiredCallbackFunctionMember() const {
    return required_callback_function_member_;
  }
  void setRequiredCallbackFunctionMember(V8VoidCallbackFunction*);

  bool hasRestrictedDoubleMember() const { return has_restricted_double_member_; }
  double restrictedDoubleMember() const {
    DCHECK(has_restricted_double_member_);
    return restricted_double_member_;
  }
  inline void setRestrictedDoubleMember(double);

  bool hasRuntimeMember() const { return has_runtime_member_; }
  bool runtimeMember() const {
    DCHECK(has_runtime_member_);
    return runtime_member_;
  }
  inline void setRuntimeMember(bool);

  bool hasRuntimeSecondMember() const { return has_runtime_second_member_; }
  bool runtimeSecondMember() const {
    DCHECK(has_runtime_second_member_);
    return runtime_second_member_;
  }
  inline void setRuntimeSecondMember(bool);

  bool hasStringMember() const { return !string_member_.IsNull(); }
  const String& stringMember() const {
    return string_member_;
  }
  inline void setStringMember(const String&);

  bool hasStringOrNullMember() const { return !string_or_null_member_.IsNull(); }
  const String& stringOrNullMember() const {
    return string_or_null_member_;
  }
  inline void setStringOrNullMember(const String&);
  inline void setStringOrNullMemberToNull();

  bool hasStringOrNullRecordMember() const { return has_string_or_null_record_member_; }
  const Vector<std::pair<String, String>>& stringOrNullRecordMember() const {
    DCHECK(has_string_or_null_record_member_);
    return string_or_null_record_member_;
  }
  void setStringOrNullRecordMember(const Vector<std::pair<String, String>>&);

  bool hasStringOrNullSequenceMember() const { return has_string_or_null_sequence_member_; }
  const Vector<String>& stringOrNullSequenceMember() const {
    DCHECK(has_string_or_null_sequence_member_);
    return string_or_null_sequence_member_;
  }
  void setStringOrNullSequenceMember(const Vector<String>&);

  bool hasStringSequenceMember() const { return has_string_sequence_member_; }
  const Vector<String>& stringSequenceMember() const {
    DCHECK(has_string_sequence_member_);
    return string_sequence_member_;
  }
  void setStringSequenceMember(const Vector<String>&);

  bool hasTestEnumOrNullOrTestEnumSequenceMember() const { return !test_enum_or_null_or_test_enum_sequence_member_.IsNull(); }
  const TestEnumOrTestEnumSequence& testEnumOrNullOrTestEnumSequenceMember() const {
    return test_enum_or_null_or_test_enum_sequence_member_;
  }
  void setTestEnumOrNullOrTestEnumSequenceMember(const TestEnumOrTestEnumSequence&);

  bool hasTestEnumOrTestEnumOrNullSequenceMember() const { return !test_enum_or_test_enum_or_null_sequence_member_.IsNull(); }
  const TestEnumOrTestEnumOrNullSequence& testEnumOrTestEnumOrNullSequenceMember() const {
    return test_enum_or_test_enum_or_null_sequence_member_;
  }
  void setTestEnumOrTestEnumOrNullSequenceMember(const TestEnumOrTestEnumOrNullSequence&);

  bool hasTestEnumOrTestEnumSequenceMember() const { return !test_enum_or_test_enum_sequence_member_.IsNull(); }
  const TestEnumOrTestEnumSequence& testEnumOrTestEnumSequenceMember() const {
    return test_enum_or_test_enum_sequence_member_;
  }
  void setTestEnumOrTestEnumSequenceMember(const TestEnumOrTestEnumSequence&);

  bool hasTestInterface2OrUint8ArrayMember() const { return !test_interface_2_or_uint8_array_member_.IsNull(); }
  const TestInterface2OrUint8Array& testInterface2OrUint8ArrayMember() const {
    return test_interface_2_or_uint8_array_member_;
  }
  void setTestInterface2OrUint8ArrayMember(const TestInterface2OrUint8Array&);

  bool hasTestInterfaceMember() const { return test_interface_member_; }
  TestInterfaceImplementation* testInterfaceMember() const {
    return test_interface_member_;
  }
  inline void setTestInterfaceMember(TestInterfaceImplementation*);

  bool hasTestInterfaceOrNullMember() const { return has_test_interface_or_null_member_; }
  TestInterfaceImplementation* testInterfaceOrNullMember() const {
    return test_interface_or_null_member_;
  }
  inline void setTestInterfaceOrNullMember(TestInterfaceImplementation*);
  inline void setTestInterfaceOrNullMemberToNull();

  bool hasTestInterfaceSequenceMember() const { return has_test_interface_sequence_member_; }
  const HeapVector<Member<TestInterfaceImplementation>>& testInterfaceSequenceMember() const {
    DCHECK(has_test_interface_sequence_member_);
    return test_interface_sequence_member_;
  }
  void setTestInterfaceSequenceMember(const HeapVector<Member<TestInterfaceImplementation>>&);

  bool hasTestObjectSequenceMember() const { return has_test_object_sequence_member_; }
  const HeapVector<Member<TestObject>>& testObjectSequenceMember() const {
    DCHECK(has_test_object_sequence_member_);
    return test_object_sequence_member_;
  }
  void setTestObjectSequenceMember(const HeapVector<Member<TestObject>>&);

  bool hasTreatNonNullObjMember() const { return treat_non_null_obj_member_; }
  V8TreatNonObjectAsNullVoidFunction* treatNonNullObjMember() const {
    return treat_non_null_obj_member_;
  }
  void setTreatNonNullObjMember(V8TreatNonObjectAsNullVoidFunction*);

  bool hasTreatNullAsStringSequenceMember() const { return has_treat_null_as_string_sequence_member_; }
  const Vector<String>& treatNullAsStringSequenceMember() const {
    DCHECK(has_treat_null_as_string_sequence_member_);
    return treat_null_as_string_sequence_member_;
  }
  void setTreatNullAsStringSequenceMember(const Vector<String>&);

  bool hasUint8ArrayMember() const { return uint8_array_member_; }
  NotShared<DOMUint8Array> uint8ArrayMember() const {
    return uint8_array_member_;
  }
  inline void setUint8ArrayMember(NotShared<DOMUint8Array>);

  bool hasUnionInRecordMember() const { return has_union_in_record_member_; }
  const HeapVector<std::pair<String, LongOrBoolean>>& unionInRecordMember() const {
    DCHECK(has_union_in_record_member_);
    return union_in_record_member_;
  }
  void setUnionInRecordMember(const HeapVector<std::pair<String, LongOrBoolean>>&);

  bool hasUnionMemberWithSequenceDefault() const { return !union_member_with_sequence_default_.IsNull(); }
  const DoubleOrDoubleSequence& unionMemberWithSequenceDefault() const {
    return union_member_with_sequence_default_;
  }
  void setUnionMemberWithSequenceDefault(const DoubleOrDoubleSequence&);

  bool hasUnionOrNullRecordMember() const { return has_union_or_null_record_member_; }
  const HeapVector<std::pair<String, DoubleOrString>>& unionOrNullRecordMember() const {
    DCHECK(has_union_or_null_record_member_);
    return union_or_null_record_member_;
  }
  void setUnionOrNullRecordMember(const HeapVector<std::pair<String, DoubleOrString>>&);

  bool hasUnionOrNullSequenceMember() const { return has_union_or_null_sequence_member_; }
  const HeapVector<DoubleOrString>& unionOrNullSequenceMember() const {
    DCHECK(has_union_or_null_sequence_member_);
    return union_or_null_sequence_member_;
  }
  void setUnionOrNullSequenceMember(const HeapVector<DoubleOrString>&);

  bool hasUnionWithAnnotatedTypeMember() const { return !union_with_annotated_type_member_.IsNull(); }
  const StringTreatNullAsEmptyStringOrLong& unionWithAnnotatedTypeMember() const {
    return union_with_annotated_type_member_;
  }
  void setUnionWithAnnotatedTypeMember(const StringTreatNullAsEmptyStringOrLong&);

  bool hasUnionWithTypedefs() const { return !union_with_typedefs_.IsNull(); }
  const FloatOrBoolean& unionWithTypedefs() const {
    return union_with_typedefs_;
  }
  void setUnionWithTypedefs(const FloatOrBoolean&);

  bool hasUnrestrictedDoubleMember() const { return has_unrestricted_double_member_; }
  double unrestrictedDoubleMember() const {
    DCHECK(has_unrestricted_double_member_);
    return unrestricted_double_member_;
  }
  inline void setUnrestrictedDoubleMember(double);

  bool hasUsvStringOrNullMember() const { return !usv_string_or_null_member_.IsNull(); }
  const String& usvStringOrNullMember() const {
    return usv_string_or_null_member_;
  }
  inline void setUsvStringOrNullMember(const String&);
  inline void setUsvStringOrNullMemberToNull();

  v8::Local<v8::Value> ToV8Impl(v8::Local<v8::Object>, v8::Isolate*) const override;
  void Trace(blink::Visitor*) override;

 private:
  bool has_any_in_record_member_ = false;
  bool has_applicable_to_type_long_member_ = false;
  bool has_boolean_member_ = false;
  bool has_create_member_ = false;
  bool has_double_or_null_member_ = false;
  bool has_double_or_null_record_member_ = false;
  bool has_double_or_null_sequence_member_ = false;
  bool has_double_or_string_sequence_member_ = false;
  bool has_element_or_null_member_ = false;
  bool has_element_or_null_record_member_ = false;
  bool has_element_or_null_sequence_member_ = false;
  bool has_enum_sequence_member_ = false;
  bool has_garbage_collected_record_member_ = false;
  bool has_internal_dictionary_sequence_member_ = false;
  bool has_is_public_ = false;
  bool has_long_member_ = false;
  bool has_member_with_hyphen_in_name_ = false;
  bool has_origin_trial_member_ = false;
  bool has_origin_trial_second_member_ = false;
  bool has_record_member_ = false;
  bool has_restricted_double_member_ = false;
  bool has_runtime_member_ = false;
  bool has_runtime_second_member_ = false;
  bool has_string_or_null_record_member_ = false;
  bool has_string_or_null_sequence_member_ = false;
  bool has_string_sequence_member_ = false;
  bool has_test_interface_or_null_member_ = false;
  bool has_test_interface_sequence_member_ = false;
  bool has_test_object_sequence_member_ = false;
  bool has_treat_null_as_string_sequence_member_ = false;
  bool has_union_in_record_member_ = false;
  bool has_union_or_null_record_member_ = false;
  bool has_union_or_null_sequence_member_ = false;
  bool has_unrestricted_double_member_ = false;

  HeapVector<std::pair<String, ScriptValue>> any_in_record_member_;
  ScriptValue any_member_;
  int32_t applicable_to_type_long_member_;
  String applicable_to_type_string_member_;
  bool boolean_member_;
  Member<V8VoidCallbackFunction> callback_function_member_;
  bool create_member_;
  Dictionary dictionary_member_;
  String dom_string_treat_null_as_empty_string_member_;
  double double_or_null_member_;
  DoubleOrDoubleOrNullSequence double_or_null_or_double_or_null_sequence_member_;
  Vector<std::pair<String, base::Optional<double>>> double_or_null_record_member_;
  Vector<base::Optional<double>> double_or_null_sequence_member_;
  DoubleOrString double_or_string_member_;
  HeapVector<DoubleOrString> double_or_string_sequence_member_;
  Member<Element> element_or_null_member_;
  HeapVector<std::pair<String, Member<Element>>> element_or_null_record_member_;
  HeapVector<Member<Element>> element_or_null_sequence_member_;
  String enum_member_;
  String enum_or_null_member_;
  Vector<String> enum_sequence_member_;
  Member<EventTarget> event_target_member_;
  HeapVector<std::pair<String, Member<TestObject>>> garbage_collected_record_member_;
  HeapVector<Member<InternalDictionary>> internal_dictionary_sequence_member_;
  bool is_public_;
  int32_t long_member_;
  bool member_with_hyphen_in_name_;
  ScriptValue object_member_;
  ScriptValue object_or_null_member_;
  bool origin_trial_member_;
  bool origin_trial_second_member_;
  DoubleOrString other_double_or_string_member_;
  Vector<std::pair<String, int8_t>> record_member_;
  Member<V8VoidCallbackFunction> required_callback_function_member_;
  double restricted_double_member_;
  bool runtime_member_;
  bool runtime_second_member_;
  String string_member_;
  String string_or_null_member_;
  Vector<std::pair<String, String>> string_or_null_record_member_;
  Vector<String> string_or_null_sequence_member_;
  Vector<String> string_sequence_member_;
  TestEnumOrTestEnumSequence test_enum_or_null_or_test_enum_sequence_member_;
  TestEnumOrTestEnumOrNullSequence test_enum_or_test_enum_or_null_sequence_member_;
  TestEnumOrTestEnumSequence test_enum_or_test_enum_sequence_member_;
  TestInterface2OrUint8Array test_interface_2_or_uint8_array_member_;
  Member<TestInterfaceImplementation> test_interface_member_;
  Member<TestInterfaceImplementation> test_interface_or_null_member_;
  HeapVector<Member<TestInterfaceImplementation>> test_interface_sequence_member_;
  HeapVector<Member<TestObject>> test_object_sequence_member_;
  Member<V8TreatNonObjectAsNullVoidFunction> treat_non_null_obj_member_;
  Vector<String> treat_null_as_string_sequence_member_;
  Member<DOMUint8Array> uint8_array_member_;
  HeapVector<std::pair<String, LongOrBoolean>> union_in_record_member_;
  DoubleOrDoubleSequence union_member_with_sequence_default_;
  HeapVector<std::pair<String, DoubleOrString>> union_or_null_record_member_;
  HeapVector<DoubleOrString> union_or_null_sequence_member_;
  StringTreatNullAsEmptyStringOrLong union_with_annotated_type_member_;
  FloatOrBoolean union_with_typedefs_;
  double unrestricted_double_member_;
  String usv_string_or_null_member_;

  friend class V8TestDictionary;
};

void TestDictionary::setApplicableToTypeLongMember(int32_t value) {
  applicable_to_type_long_member_ = value;
  has_applicable_to_type_long_member_ = true;
}

void TestDictionary::setApplicableToTypeStringMember(const String& value) {
  applicable_to_type_string_member_ = value;
}

void TestDictionary::setBooleanMember(bool value) {
  boolean_member_ = value;
  has_boolean_member_ = true;
}

void TestDictionary::setCreateMember(bool value) {
  create_member_ = value;
  has_create_member_ = true;
}

void TestDictionary::setDomStringTreatNullAsEmptyStringMember(const String& value) {
  dom_string_treat_null_as_empty_string_member_ = value;
}

void TestDictionary::setDoubleOrNullMember(double value) {
  double_or_null_member_ = value;
  has_double_or_null_member_ = true;
}

void TestDictionary::setDoubleOrNullMemberToNull() {
  has_double_or_null_member_ = false;
}

void TestDictionary::setElementOrNullMember(Element* value) {
  element_or_null_member_ = value;
  has_element_or_null_member_ = true;
}

void TestDictionary::setElementOrNullMemberToNull() {
  element_or_null_member_ = Member<Element>();
  has_element_or_null_member_ = true;
}

void TestDictionary::setEnumMember(const String& value) {
  enum_member_ = value;
}

void TestDictionary::setEnumOrNullMember(const String& value) {
  enum_or_null_member_ = value;
}

void TestDictionary::setEnumOrNullMemberToNull() {
  enum_or_null_member_ = String();
}

void TestDictionary::setEventTargetMember(EventTarget* value) {
  event_target_member_ = value;
}

void TestDictionary::setIsPublic(bool value) {
  is_public_ = value;
  has_is_public_ = true;
}

void TestDictionary::setLongMember(int32_t value) {
  long_member_ = value;
  has_long_member_ = true;
}

void TestDictionary::setMemberWithHyphenInName(bool value) {
  member_with_hyphen_in_name_ = value;
  has_member_with_hyphen_in_name_ = true;
}

void TestDictionary::setOriginTrialMember(bool value) {
  origin_trial_member_ = value;
  has_origin_trial_member_ = true;
}

void TestDictionary::setOriginTrialSecondMember(bool value) {
  origin_trial_second_member_ = value;
  has_origin_trial_second_member_ = true;
}

void TestDictionary::setRestrictedDoubleMember(double value) {
  restricted_double_member_ = value;
  has_restricted_double_member_ = true;
}

void TestDictionary::setRuntimeMember(bool value) {
  runtime_member_ = value;
  has_runtime_member_ = true;
}

void TestDictionary::setRuntimeSecondMember(bool value) {
  runtime_second_member_ = value;
  has_runtime_second_member_ = true;
}

void TestDictionary::setStringMember(const String& value) {
  string_member_ = value;
}

void TestDictionary::setStringOrNullMember(const String& value) {
  string_or_null_member_ = value;
}

void TestDictionary::setStringOrNullMemberToNull() {
  string_or_null_member_ = String();
}

void TestDictionary::setTestInterfaceMember(TestInterfaceImplementation* value) {
  test_interface_member_ = value;
}

void TestDictionary::setTestInterfaceOrNullMember(TestInterfaceImplementation* value) {
  test_interface_or_null_member_ = value;
  has_test_interface_or_null_member_ = true;
}

void TestDictionary::setTestInterfaceOrNullMemberToNull() {
  test_interface_or_null_member_ = Member<TestInterfaceImplementation>();
  has_test_interface_or_null_member_ = true;
}

void TestDictionary::setUint8ArrayMember(NotShared<DOMUint8Array> value) {
  uint8_array_member_ = value.View();
}

void TestDictionary::setUnrestrictedDoubleMember(double value) {
  unrestricted_double_member_ = value;
  has_unrestricted_double_member_ = true;
}

void TestDictionary::setUsvStringOrNullMember(const String& value) {
  usv_string_or_null_member_ = value;
}

void TestDictionary::setUsvStringOrNullMemberToNull() {
  usv_string_or_null_member_ = String();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_DICTIONARY_H_
