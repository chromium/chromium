// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DICTIONARY_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DICTIONARY_TEST_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_string.h"
#include "third_party/blink/renderer/bindings/core/v8/internal_enum_or_internal_enum_sequence.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internal_enum.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_callback.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class InternalDictionary;
class InternalDictionaryDerived;
class InternalDictionaryDerivedDerived;

class DictionaryTest : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DictionaryTest();
  ~DictionaryTest() override;

  // Stores all members into corresponding fields
  void set(const InternalDictionary*);
  // Sets each member of the given TestDictionary from fields
  InternalDictionary* get(v8::Isolate* isolate);

  void setDerived(const InternalDictionaryDerived*);
  InternalDictionaryDerived* getDerived(v8::Isolate* isolate);

  void setDerivedDerived(const InternalDictionaryDerivedDerived*);
  InternalDictionaryDerivedDerived* getDerivedDerived(v8::Isolate* isolate);

  void Trace(Visitor*) const override;

 private:
  void Reset();

  void GetInternals(InternalDictionary*);
  void GetDerivedInternals(InternalDictionaryDerived*);
  void GetDerivedDerivedInternals(InternalDictionaryDerivedDerived*);

  // The reason to use absl::optional<T> is convenience; we use
  // absl::optional<T> here to record whether the member field is set or not.
  // Some members are not wrapped with optional because:
  //  - |longMemberWithDefault| has a non-null default value
  //  - String and PtrTypes can express whether they are null
  //  - absl::optional does not work with GarbageCollected types when used on
  //  heap.
  absl::optional<int> long_member_;
  absl::optional<int> long_member_with_clamp_;
  absl::optional<int> long_member_with_enforce_range_;
  int long_member_with_default_;
  absl::optional<int> long_or_null_member_;
  absl::optional<int> long_or_null_member_with_default_;
  absl::optional<bool> boolean_member_;
  absl::optional<double> double_member_;
  absl::optional<double> unrestricted_double_member_;
  absl::optional<String> string_member_;
  String string_member_with_default_;
  absl::optional<String> byte_string_member_;
  absl::optional<String> usv_string_member_;
  absl::optional<Vector<String>> string_sequence_member_;
  Vector<String> string_sequence_member_with_default_;
  absl::optional<Vector<String>> string_sequence_or_null_member_;
  absl::optional<String> enum_member_;
  String enum_member_with_default_;
#ifdef USE_BLINK_V8_BINDING_NEW_IDL_DICTIONARY
  // The outer optional<> represents if the member is missing, and the inner
  // optional<> represents if the member is a null value.
  absl::optional<absl::optional<V8InternalEnum>> enum_or_null_member_;
#else
  absl::optional<String> enum_or_null_member_;
#endif
  Member<Element> element_member_;
  Member<Element> element_or_null_member_;
  bool has_element_or_null_member_ = false;
  ScriptValue object_member_;
  ScriptValue object_or_null_member_with_default_;
  DoubleOrString double_or_string_member_;
  Member<HeapVector<DoubleOrString>> double_or_string_sequence_or_null_member_;
  Member<EventTarget> event_target_or_null_member_;
  absl::optional<String> derived_string_member_;
  String derived_string_member_with_default_;
  absl::optional<String> derived_derived_string_member_;
  bool required_boolean_member_;
  absl::optional<HashMap<String, String>> dictionary_member_properties_;
  InternalEnumOrInternalEnumSequence internal_enum_or_internal_enum_sequence_;
  ScriptValue any_member_;
  Member<V8TestCallback> callback_function_member_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DICTIONARY_TEST_H_
