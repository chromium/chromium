// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_impl.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/test_dictionary.h"

#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_implementation.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_object.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TestDictionary::TestDictionary() {
  setDoubleOrStringMember(DoubleOrString::FromDouble(3.14));
  setEnumMember("foo");
  setLongMember(1);
  setMemberWithHyphenInName(false);
  setOtherDoubleOrStringMember(DoubleOrString::FromString("default string value"));
  setRestrictedDoubleMember(3.14);
  setStringOrNullMember("default string value");
  setStringSequenceMember(Vector<String>());
  setTestInterfaceSequenceMember(HeapVector<Member<TestInterfaceImplementation>>());
  setTreatNullAsStringSequenceMember(Vector<String>());
  setUnionMemberWithSequenceDefault(DoubleOrDoubleSequence::FromDoubleSequence(Vector<double>()));
  setUnrestrictedDoubleMember(3.14);
}

TestDictionary::~TestDictionary() = default;

void TestDictionary::setAnyInRecordMember(const HeapVector<std::pair<String, ScriptValue>>& value) {
  any_in_record_member_ = value;
  has_any_in_record_member_ = true;
}

void TestDictionary::setAnyMember(ScriptValue value) {
  any_member_ = value;
}

void TestDictionary::setCallbackFunctionMember(V8VoidCallbackFunction* value) {
  callback_function_member_ = value;
}

void TestDictionary::setDictionaryMember(Dictionary value) {
  dictionary_member_ = value;
}

void TestDictionary::setDoubleOrNullOrDoubleOrNullSequenceMember(const DoubleOrDoubleOrNullSequence& value) {
  double_or_null_or_double_or_null_sequence_member_ = value;
}

void TestDictionary::setDoubleOrNullRecordMember(const Vector<std::pair<String, base::Optional<double>>>& value) {
  double_or_null_record_member_ = value;
  has_double_or_null_record_member_ = true;
}

void TestDictionary::setDoubleOrNullSequenceMember(const Vector<base::Optional<double>>& value) {
  double_or_null_sequence_member_ = value;
  has_double_or_null_sequence_member_ = true;
}

void TestDictionary::setDoubleOrStringMember(const DoubleOrString& value) {
  double_or_string_member_ = value;
}

void TestDictionary::setDoubleOrStringSequenceMember(const HeapVector<DoubleOrString>& value) {
  double_or_string_sequence_member_ = value;
  has_double_or_string_sequence_member_ = true;
}

void TestDictionary::setElementOrNullRecordMember(const HeapVector<std::pair<String, Member<Element>>>& value) {
  element_or_null_record_member_ = value;
  has_element_or_null_record_member_ = true;
}

void TestDictionary::setElementOrNullSequenceMember(const HeapVector<Member<Element>>& value) {
  element_or_null_sequence_member_ = value;
  has_element_or_null_sequence_member_ = true;
}

void TestDictionary::setEnumSequenceMember(const Vector<String>& value) {
  enum_sequence_member_ = value;
  has_enum_sequence_member_ = true;
}

void TestDictionary::setGarbageCollectedRecordMember(const HeapVector<std::pair<String, Member<TestObject>>>& value) {
  garbage_collected_record_member_ = value;
  has_garbage_collected_record_member_ = true;
}

void TestDictionary::setInternalDictionarySequenceMember(const HeapVector<Member<InternalDictionary>>& value) {
  internal_dictionary_sequence_member_ = value;
  has_internal_dictionary_sequence_member_ = true;
}

void TestDictionary::setObjectMember(ScriptValue value) {
  object_member_ = value;
}

void TestDictionary::setObjectOrNullMember(ScriptValue value) {
  object_or_null_member_ = value;
}

void TestDictionary::setObjectOrNullMemberToNull() {
  object_or_null_member_ = ScriptValue();
}

void TestDictionary::setOtherDoubleOrStringMember(const DoubleOrString& value) {
  other_double_or_string_member_ = value;
}

void TestDictionary::setRecordMember(const Vector<std::pair<String, int8_t>>& value) {
  record_member_ = value;
  has_record_member_ = true;
}

void TestDictionary::setRequiredCallbackFunctionMember(V8VoidCallbackFunction* value) {
  required_callback_function_member_ = value;
}

void TestDictionary::setStringOrNullRecordMember(const Vector<std::pair<String, String>>& value) {
  string_or_null_record_member_ = value;
  has_string_or_null_record_member_ = true;
}

void TestDictionary::setStringOrNullSequenceMember(const Vector<String>& value) {
  string_or_null_sequence_member_ = value;
  has_string_or_null_sequence_member_ = true;
}

void TestDictionary::setStringSequenceMember(const Vector<String>& value) {
  string_sequence_member_ = value;
  has_string_sequence_member_ = true;
}

void TestDictionary::setTestEnumOrNullOrTestEnumSequenceMember(const TestEnumOrTestEnumSequence& value) {
  test_enum_or_null_or_test_enum_sequence_member_ = value;
}

void TestDictionary::setTestEnumOrTestEnumOrNullSequenceMember(const TestEnumOrTestEnumOrNullSequence& value) {
  test_enum_or_test_enum_or_null_sequence_member_ = value;
}

void TestDictionary::setTestEnumOrTestEnumSequenceMember(const TestEnumOrTestEnumSequence& value) {
  test_enum_or_test_enum_sequence_member_ = value;
}

void TestDictionary::setTestInterface2OrUint8ArrayMember(const TestInterface2OrUint8Array& value) {
  test_interface_2_or_uint8_array_member_ = value;
}

void TestDictionary::setTestInterfaceSequenceMember(const HeapVector<Member<TestInterfaceImplementation>>& value) {
  test_interface_sequence_member_ = value;
  has_test_interface_sequence_member_ = true;
}

void TestDictionary::setTestObjectSequenceMember(const HeapVector<Member<TestObject>>& value) {
  test_object_sequence_member_ = value;
  has_test_object_sequence_member_ = true;
}

void TestDictionary::setTreatNonNullObjMember(V8TreatNonObjectAsNullVoidFunction* value) {
  treat_non_null_obj_member_ = value;
}

void TestDictionary::setTreatNullAsStringSequenceMember(const Vector<String>& value) {
  treat_null_as_string_sequence_member_ = value;
  has_treat_null_as_string_sequence_member_ = true;
}

void TestDictionary::setUnionInRecordMember(const HeapVector<std::pair<String, LongOrBoolean>>& value) {
  union_in_record_member_ = value;
  has_union_in_record_member_ = true;
}

void TestDictionary::setUnionMemberWithSequenceDefault(const DoubleOrDoubleSequence& value) {
  union_member_with_sequence_default_ = value;
}

void TestDictionary::setUnionOrNullRecordMember(const HeapVector<std::pair<String, DoubleOrString>>& value) {
  union_or_null_record_member_ = value;
  has_union_or_null_record_member_ = true;
}

void TestDictionary::setUnionOrNullSequenceMember(const HeapVector<DoubleOrString>& value) {
  union_or_null_sequence_member_ = value;
  has_union_or_null_sequence_member_ = true;
}

void TestDictionary::setUnionWithAnnotatedTypeMember(const StringTreatNullAsEmptyStringOrLong& value) {
  union_with_annotated_type_member_ = value;
}

void TestDictionary::setUnionWithTypedefs(const FloatOrBoolean& value) {
  union_with_typedefs_ = value;
}

void TestDictionary::Trace(blink::Visitor* visitor) {
  visitor->Trace(any_in_record_member_);
  visitor->Trace(any_member_);
  visitor->Trace(callback_function_member_);
  visitor->Trace(double_or_null_or_double_or_null_sequence_member_);
  visitor->Trace(double_or_string_member_);
  visitor->Trace(double_or_string_sequence_member_);
  visitor->Trace(element_or_null_member_);
  visitor->Trace(element_or_null_record_member_);
  visitor->Trace(element_or_null_sequence_member_);
  visitor->Trace(event_target_member_);
  visitor->Trace(garbage_collected_record_member_);
  visitor->Trace(internal_dictionary_sequence_member_);
  visitor->Trace(object_member_);
  visitor->Trace(object_or_null_member_);
  visitor->Trace(other_double_or_string_member_);
  visitor->Trace(required_callback_function_member_);
  visitor->Trace(test_enum_or_null_or_test_enum_sequence_member_);
  visitor->Trace(test_enum_or_test_enum_or_null_sequence_member_);
  visitor->Trace(test_enum_or_test_enum_sequence_member_);
  visitor->Trace(test_interface_2_or_uint8_array_member_);
  visitor->Trace(test_interface_member_);
  visitor->Trace(test_interface_or_null_member_);
  visitor->Trace(test_interface_sequence_member_);
  visitor->Trace(test_object_sequence_member_);
  visitor->Trace(treat_non_null_obj_member_);
  visitor->Trace(uint8_array_member_);
  visitor->Trace(union_in_record_member_);
  visitor->Trace(union_member_with_sequence_default_);
  visitor->Trace(union_or_null_record_member_);
  visitor->Trace(union_or_null_sequence_member_);
  visitor->Trace(union_with_annotated_type_member_);
  visitor->Trace(union_with_typedefs_);
  IDLDictionaryBase::Trace(visitor);
}

}  // namespace blink
