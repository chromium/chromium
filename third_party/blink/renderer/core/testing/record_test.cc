// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/record_test.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_bytestringbytestringrecord.h"

namespace blink {

RecordTest::RecordTest() = default;

RecordTest::~RecordTest() = default;

void RecordTest::setStringLongRecord(
    const Vector<std::pair<String, int32_t>>& arg) {
  string_long_record_ = arg;
}

Vector<std::pair<String, int32_t>> RecordTest::getStringLongRecord() {
  return string_long_record_;
}

void RecordTest::setNullableStringLongRecord(
    const std::optional<Vector<std::pair<String, int32_t>>>& arg) {
  nullable_string_long_record_ = arg;
}

std::optional<Vector<std::pair<String, int32_t>>>
RecordTest::getNullableStringLongRecord() {
  return nullable_string_long_record_;
}

Vector<std::pair<String, String>> RecordTest::GetByteStringByteStringRecord() {
  return byte_string_byte_string_record_;
}

void RecordTest::setByteStringByteStringRecord(
    const Vector<std::pair<String, String>>& arg) {
  byte_string_byte_string_record_ = arg;
}

void RecordTest::setStringElementRecord(
    const HeapVector<std::pair<String, Member<Element>>>& arg) {
  string_element_record_ = arg;
}

HeapVector<std::pair<String, Member<Element>>>
RecordTest::getStringElementRecord() {
  return string_element_record_;
}

void RecordTest::setUSVStringUSVStringBooleanRecordRecord(
    const RecordTest::NestedRecordType& arg) {
  usv_string_usv_string_boolean_record_record_ = arg;
}

RecordTest::NestedRecordType
RecordTest::getUSVStringUSVStringBooleanRecordRecord() {
  return usv_string_usv_string_boolean_record_record_;
}

Vector<std::pair<String, Vector<String>>>
RecordTest::returnStringByteStringSequenceRecord() {
  Vector<std::pair<String, Vector<String>>> record;
  Vector<String> inner_vector1;
  inner_vector1.push_back("hello, world");
  inner_vector1.push_back("hi, mom");
  record.push_back(std::make_pair(String("foo"), inner_vector1));
  Vector<String> inner_vector2;
  inner_vector2.push_back("goodbye, mom");
  record.push_back(std::make_pair(String("bar"), inner_vector2));
  return record;
}

bool RecordTest::unionReceivedARecord(
    const V8UnionBooleanOrByteStringByteStringRecord* arg) {
  return arg->IsByteStringByteStringRecord();
}

void RecordTest::Trace(Visitor* visitor) const {
  visitor->Trace(string_element_record_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
