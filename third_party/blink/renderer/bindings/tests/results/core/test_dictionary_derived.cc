// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_impl.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/test_dictionary_derived.h"

namespace blink {

TestDictionaryDerivedImplementedAs::TestDictionaryDerivedImplementedAs() {
  setDerivedStringMemberWithDefault("default string value");
}

TestDictionaryDerivedImplementedAs::~TestDictionaryDerivedImplementedAs() {}

TestDictionaryDerivedImplementedAs::TestDictionaryDerivedImplementedAs(const TestDictionaryDerivedImplementedAs&) = default;

TestDictionaryDerivedImplementedAs& TestDictionaryDerivedImplementedAs::operator=(const TestDictionaryDerivedImplementedAs&) = default;

void TestDictionaryDerivedImplementedAs::setStringOrDoubleSequenceMember(const HeapVector<StringOrDouble>& value) {
  string_or_double_sequence_member_ = value;
  has_string_or_double_sequence_member_ = true;
}

void TestDictionaryDerivedImplementedAs::Trace(blink::Visitor* visitor) {
  visitor->Trace(string_or_double_sequence_member_);
  TestDictionary::Trace(visitor);
}

}  // namespace blink
