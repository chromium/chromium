// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_impl.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_DICTIONARY_DERIVED_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_DICTIONARY_DERIVED_H_

#include "third_party/blink/renderer/bindings/core/v8/string_or_double.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_dictionary.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT TestDictionaryDerivedImplementedAs : public TestDictionary {
 public:
  static TestDictionaryDerivedImplementedAs* Create() { return MakeGarbageCollected<TestDictionaryDerivedImplementedAs>(); }
  static TestDictionaryDerivedImplementedAs* Create(v8::Isolate* isolate) {
    return MakeGarbageCollected<TestDictionaryDerivedImplementedAs>();
  }

  TestDictionaryDerivedImplementedAs();
  ~TestDictionaryDerivedImplementedAs() override;

  bool hasDerivedStringMember() const { return !derived_string_member_.IsNull(); }
  const String& derivedStringMember() const {
    return derived_string_member_;
  }
  inline void setDerivedStringMember(const String&);

  bool hasDerivedStringMemberWithDefault() const { return !derived_string_member_with_default_.IsNull(); }
  const String& derivedStringMemberWithDefault() const {
    return derived_string_member_with_default_;
  }
  inline void setDerivedStringMemberWithDefault(const String&);

  bool hasRequiredLongMember() const { return has_required_long_member_; }
  int32_t requiredLongMember() const {
    DCHECK(has_required_long_member_);
    return required_long_member_;
  }
  inline void setRequiredLongMember(int32_t);

  bool hasStringOrDoubleSequenceMember() const { return has_string_or_double_sequence_member_; }
  const HeapVector<StringOrDouble>& stringOrDoubleSequenceMember() const {
    DCHECK(has_string_or_double_sequence_member_);
    return string_or_double_sequence_member_;
  }
  void setStringOrDoubleSequenceMember(const HeapVector<StringOrDouble>&);

  v8::Local<v8::Value> ToV8Impl(v8::Local<v8::Object>, v8::Isolate*) const override;
  void Trace(Visitor*) const override;

 private:
  bool has_required_long_member_ = false;
  bool has_string_or_double_sequence_member_ = false;

  String derived_string_member_;
  String derived_string_member_with_default_;
  int32_t required_long_member_;
  HeapVector<StringOrDouble> string_or_double_sequence_member_;

  friend class V8TestDictionaryDerivedImplementedAs;
};

void TestDictionaryDerivedImplementedAs::setDerivedStringMember(const String& value) {
  derived_string_member_ = value;
}

void TestDictionaryDerivedImplementedAs::setDerivedStringMemberWithDefault(const String& value) {
  derived_string_member_with_default_ = value;
}

void TestDictionaryDerivedImplementedAs::setRequiredLongMember(int32_t value) {
  required_long_member_ = value;
  has_required_long_member_ = true;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_DICTIONARY_DERIVED_H_
