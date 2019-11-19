// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_impl.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/modules/test_dictionary_2.h"

namespace blink {

TestDictionary2::TestDictionary2() {
  setDefaultEmptyDictionary(MakeGarbageCollected<TestDictionary>());
  setDefaultEmptyDictionaryForUnion(TestDictionaryOrLong::FromTestDictionary(MakeGarbageCollected<TestDictionary>()));
}

TestDictionary2::~TestDictionary2() = default;

void TestDictionary2::setDefaultEmptyDictionary(TestDictionary* value) {
  default_empty_dictionary_ = value;
}

void TestDictionary2::setDefaultEmptyDictionaryForUnion(const TestDictionaryOrLong& value) {
  default_empty_dictionary_for_union_ = value;
}

void TestDictionary2::Trace(blink::Visitor* visitor) {
  visitor->Trace(default_empty_dictionary_);
  visitor->Trace(default_empty_dictionary_for_union_);
  IDLDictionaryBase::Trace(visitor);
}

}  // namespace blink
