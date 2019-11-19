// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_impl.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_MODULES_TEST_DICTIONARY_2_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_MODULES_TEST_DICTIONARY_2_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_dictionary_base.h"
#include "third_party/blink/renderer/bindings/modules/v8/test_dictionary_or_long.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_dictionary.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class MODULES_EXPORT TestDictionary2 : public IDLDictionaryBase {
 public:
  static TestDictionary2* Create() { return MakeGarbageCollected<TestDictionary2>(); }

  TestDictionary2();
  virtual ~TestDictionary2();

  bool hasDefaultEmptyDictionary() const { return default_empty_dictionary_; }
  TestDictionary* defaultEmptyDictionary() const {
    return default_empty_dictionary_;
  }
  void setDefaultEmptyDictionary(TestDictionary*);

  bool hasDefaultEmptyDictionaryForUnion() const { return !default_empty_dictionary_for_union_.IsNull(); }
  const TestDictionaryOrLong& defaultEmptyDictionaryForUnion() const {
    return default_empty_dictionary_for_union_;
  }
  void setDefaultEmptyDictionaryForUnion(const TestDictionaryOrLong&);

  v8::Local<v8::Value> ToV8Impl(v8::Local<v8::Object>, v8::Isolate*) const override;
  void Trace(blink::Visitor*) override;

 private:

  Member<TestDictionary> default_empty_dictionary_;
  TestDictionaryOrLong default_empty_dictionary_for_union_;

  friend class V8TestDictionary2;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_MODULES_TEST_DICTIONARY_2_H_
