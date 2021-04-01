// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_impl.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_PERMISSIVE_DICTIONARY_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_PERMISSIVE_DICTIONARY_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_dictionary_base.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CORE_EXPORT TestPermissiveDictionary : public IDLDictionaryBase {
 public:
  static TestPermissiveDictionary* Create() { return MakeGarbageCollected<TestPermissiveDictionary>(); }
  static TestPermissiveDictionary* Create(v8::Isolate* isolate) {
    return MakeGarbageCollected<TestPermissiveDictionary>();
  }

  TestPermissiveDictionary();
  ~TestPermissiveDictionary() override;

  bool hasBooleanMember() const { return has_boolean_member_; }
  bool booleanMember() const {
    DCHECK(has_boolean_member_);
    return boolean_member_;
  }
  inline void setBooleanMember(bool);

  v8::Local<v8::Value> ToV8Impl(v8::Local<v8::Object>, v8::Isolate*) const override;
  void Trace(Visitor*) const override;

 private:
  bool has_boolean_member_ = false;

  bool boolean_member_;

  friend class V8TestPermissiveDictionary;
};

void TestPermissiveDictionary::setBooleanMember(bool value) {
  boolean_member_ = value;
  has_boolean_member_ = true;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_PERMISSIVE_DICTIONARY_H_
