// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DICTIONARY_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DICTIONARY_TEST_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_string.h"
#include "third_party/blink/renderer/bindings/core/v8/internal_enum_or_internal_enum_sequence.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_callback.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class InternalDictionary;
class InternalDictionaryDerived;
class InternalDictionaryDerivedDerived;
class ScriptState;

class DictionaryTest : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DictionaryTest();
  ~DictionaryTest() override;

  // Stores all members into corresponding fields
  void set(const InternalDictionary*);
  // Sets each member of the given TestDictionary from fields
  InternalDictionary* get();
  // Returns properties of the latest |dictionaryMember| which was set via
  // set().
  ScriptValue getDictionaryMemberProperties(ScriptState*);

  void setDerived(const InternalDictionaryDerived*);
  InternalDictionaryDerived* getDerived();

  void setDerivedDerived(const InternalDictionaryDerivedDerived*);
  InternalDictionaryDerivedDerived* getDerivedDerived();

  String stringFromIterable(ScriptState*,
                            Dictionary iterable,
                            ExceptionState&) const;

  void Trace(blink::Visitor*) override;

 private:
  void Reset();

  void GetInternals(InternalDictionary*);
  void GetDerivedInternals(InternalDictionaryDerived*);
  void GetDerivedDerivedInternals(InternalDictionaryDerivedDerived*);

  // The reason to use base::Optional<T> is convenience; we use
  // base::Optional<T> here to record whether the member field is set or not.
  // Some members are not wrapped with Optional because:
  //  - |longMemberWithDefault| has a non-null default value
  //  - String and PtrTypes can express whether they are null
  base::Optional<int> long_member_;
  base::Optional<int> long_member_with_clamp_;
  base::Optional<int> long_member_with_enforce_range_;
  int long_member_with_default_;
  base::Optional<int> long_or_null_member_;
  base::Optional<int> long_or_null_member_with_default_;
  base::Optional<bool> boolean_member_;
  base::Optional<double> double_member_;
  base::Optional<double> unrestricted_double_member_;
  String string_member_;
  String string_member_with_default_;
  String byte_string_member_;
  String usv_string_member_;
  base::Optional<Vector<String>> string_sequence_member_;
  Vector<String> string_sequence_member_with_default_;
  base::Optional<Vector<String>> string_sequence_or_null_member_;
  String enum_member_;
  String enum_member_with_default_;
  String enum_or_null_member_;
  Member<Element> element_member_;
  base::Optional<Member<Element>> element_or_null_member_;
  ScriptValue object_member_;
  ScriptValue object_or_null_member_with_default_;
  DoubleOrString double_or_string_member_;
  base::Optional<HeapVector<DoubleOrString>> double_or_string_sequence_member_;
  Member<EventTarget> event_target_or_null_member_;
  String derived_string_member_;
  String derived_string_member_with_default_;
  String derived_derived_string_member_;
  bool required_boolean_member_;
  base::Optional<HashMap<String, String>> dictionary_member_properties_;
  InternalEnumOrInternalEnumSequence internal_enum_or_internal_enum_sequence_;
  ScriptValue any_member_;
  Member<V8TestCallback> callback_function_member_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DICTIONARY_TEST_H_
