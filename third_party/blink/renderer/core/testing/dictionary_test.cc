// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/dictionary_test.h"

#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internal_dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internal_dictionary_derived.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internal_dictionary_derived_derived.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

DictionaryTest::DictionaryTest() : required_boolean_member_(false) {}

DictionaryTest::~DictionaryTest() = default;

void DictionaryTest::set(const InternalDictionary* testing_dictionary) {
  Reset();
  if (testing_dictionary->hasLongMember())
    long_member_ = testing_dictionary->longMember();
  if (testing_dictionary->hasLongMemberWithClamp())
    long_member_with_clamp_ = testing_dictionary->longMemberWithClamp();
  if (testing_dictionary->hasLongMemberWithEnforceRange()) {
    long_member_with_enforce_range_ =
        testing_dictionary->longMemberWithEnforceRange();
  }
  long_member_with_default_ = testing_dictionary->longMemberWithDefault();
  if (testing_dictionary->hasLongOrNullMember())
    long_or_null_member_ = testing_dictionary->longOrNullMember();
  if (testing_dictionary->hasLongOrNullMemberWithDefault()) {
    long_or_null_member_with_default_ =
        testing_dictionary->longOrNullMemberWithDefault();
  }
  if (testing_dictionary->hasBooleanMember())
    boolean_member_ = testing_dictionary->booleanMember();
  if (testing_dictionary->hasDoubleMember())
    double_member_ = testing_dictionary->doubleMember();
  if (testing_dictionary->hasUnrestrictedDoubleMember()) {
    unrestricted_double_member_ =
        testing_dictionary->unrestrictedDoubleMember();
  }
  if (testing_dictionary->hasStringMember())
    string_member_ = testing_dictionary->stringMember();
  string_member_with_default_ = testing_dictionary->stringMemberWithDefault();
  if (testing_dictionary->hasByteStringMember())
    byte_string_member_ = testing_dictionary->byteStringMember();
  if (testing_dictionary->hasUsvStringMember())
    usv_string_member_ = testing_dictionary->usvStringMember();
  if (testing_dictionary->hasStringSequenceMember())
    string_sequence_member_ = testing_dictionary->stringSequenceMember();
  string_sequence_member_with_default_ =
      testing_dictionary->stringSequenceMemberWithDefault();
  if (testing_dictionary->hasStringSequenceOrNullMember()) {
    string_sequence_or_null_member_ =
        testing_dictionary->stringSequenceOrNullMember();
  }
  if (testing_dictionary->hasEnumMember())
    enum_member_ = testing_dictionary->enumMember();
  enum_member_with_default_ = testing_dictionary->enumMemberWithDefault();
  if (testing_dictionary->hasEnumOrNullMember())
    enum_or_null_member_ = testing_dictionary->enumOrNullMember();
  if (testing_dictionary->hasElementMember())
    element_member_ = testing_dictionary->elementMember();
  if (testing_dictionary->hasElementOrNullMember()) {
    element_or_null_member_ = testing_dictionary->elementOrNullMember();
    has_element_or_null_member_ = true;
  }
  if (testing_dictionary->hasObjectMember())
    object_member_ = testing_dictionary->objectMember();
  object_or_null_member_with_default_ =
      testing_dictionary->objectOrNullMemberWithDefault();
  if (testing_dictionary->hasDoubleOrStringMember())
    double_or_string_member_ = testing_dictionary->doubleOrStringMember();
  if (testing_dictionary->hasDoubleOrStringSequenceMember()) {
    double_or_string_sequence_or_null_member_ =
        MakeGarbageCollected<HeapVector<DoubleOrString>>();
    *double_or_string_sequence_or_null_member_ =
        testing_dictionary->doubleOrStringSequenceMember();
  }
  // eventTargetOrNullMember has a default null value.
  event_target_or_null_member_ = testing_dictionary->eventTargetOrNullMember();
  if (testing_dictionary->hasInternalEnumOrInternalEnumSequenceMember()) {
    internal_enum_or_internal_enum_sequence_ =
        testing_dictionary->internalEnumOrInternalEnumSequenceMember();
  }
  if (testing_dictionary->hasAnyMember())
    any_member_ = testing_dictionary->anyMember();
  if (testing_dictionary->hasCallbackFunctionMember())
    callback_function_member_ = testing_dictionary->callbackFunctionMember();
}

InternalDictionary* DictionaryTest::get(v8::Isolate* isolate) {
  InternalDictionary* result = InternalDictionary::Create(isolate);
  GetInternals(result);
  return result;
}

void DictionaryTest::setDerived(const InternalDictionaryDerived* derived) {
  DCHECK(derived->hasRequiredBooleanMember());
  set(derived);
  if (derived->hasDerivedStringMember())
    derived_string_member_ = derived->derivedStringMember();
  derived_string_member_with_default_ =
      derived->derivedStringMemberWithDefault();
  required_boolean_member_ = derived->requiredBooleanMember();
}

InternalDictionaryDerived* DictionaryTest::getDerived(v8::Isolate* isolate) {
  InternalDictionaryDerived* result =
      InternalDictionaryDerived::Create(isolate);
  GetDerivedInternals(result);
  return result;
}

void DictionaryTest::setDerivedDerived(
    const InternalDictionaryDerivedDerived* derived) {
  setDerived(derived);
  if (derived->hasDerivedDerivedStringMember())
    derived_derived_string_member_ = derived->derivedDerivedStringMember();
}

InternalDictionaryDerivedDerived* DictionaryTest::getDerivedDerived(
    v8::Isolate* isolate) {
  InternalDictionaryDerivedDerived* result =
      InternalDictionaryDerivedDerived::Create(isolate);
  GetDerivedDerivedInternals(result);
  return result;
}

void DictionaryTest::Reset() {
  long_member_ = base::nullopt;
  long_member_with_clamp_ = base::nullopt;
  long_member_with_enforce_range_ = base::nullopt;
  long_member_with_default_ = -1;  // This value should not be returned.
  long_or_null_member_ = base::nullopt;
  long_or_null_member_with_default_ = base::nullopt;
  boolean_member_ = base::nullopt;
  double_member_ = base::nullopt;
  unrestricted_double_member_ = base::nullopt;
  string_member_ = base::nullopt;
  string_member_with_default_ = String("Should not be returned");
  byte_string_member_ = base::nullopt;
  usv_string_member_ = base::nullopt;
  string_sequence_member_ = base::nullopt;
  string_sequence_member_with_default_.Fill("Should not be returned", 1);
  string_sequence_or_null_member_ = base::nullopt;
  enum_member_ = base::nullopt;
  enum_member_with_default_ = String();
  enum_or_null_member_ = base::nullopt;
  element_member_ = nullptr;
  element_or_null_member_.Clear();
  has_element_or_null_member_ = false;
  object_member_ = ScriptValue();
  object_or_null_member_with_default_ = ScriptValue();
  double_or_string_member_ = DoubleOrString();
  double_or_string_sequence_or_null_member_ = nullptr;
  event_target_or_null_member_ = nullptr;
  derived_string_member_ = base::nullopt;
  derived_string_member_with_default_ = String();
  derived_derived_string_member_ = base::nullopt;
  required_boolean_member_ = false;
  dictionary_member_properties_ = base::nullopt;
  internal_enum_or_internal_enum_sequence_ =
      InternalEnumOrInternalEnumSequence();
  any_member_ = ScriptValue();
  callback_function_member_ = nullptr;
}

void DictionaryTest::GetInternals(InternalDictionary* dict) {
  DCHECK(dict);

  if (long_member_)
    dict->setLongMember(long_member_.value());
  if (long_member_with_clamp_)
    dict->setLongMemberWithClamp(long_member_with_clamp_.value());
  if (long_member_with_enforce_range_) {
    dict->setLongMemberWithEnforceRange(
        long_member_with_enforce_range_.value());
  }
  dict->setLongMemberWithDefault(long_member_with_default_);
  if (long_or_null_member_)
    dict->setLongOrNullMember(long_or_null_member_.value());
  if (long_or_null_member_with_default_) {
    dict->setLongOrNullMemberWithDefault(
        long_or_null_member_with_default_.value());
  }
  if (boolean_member_)
    dict->setBooleanMember(boolean_member_.value());
  if (double_member_)
    dict->setDoubleMember(double_member_.value());
  if (unrestricted_double_member_)
    dict->setUnrestrictedDoubleMember(unrestricted_double_member_.value());
  if (string_member_)
    dict->setStringMember(string_member_.value());
  dict->setStringMemberWithDefault(string_member_with_default_);
  if (byte_string_member_)
    dict->setByteStringMember(byte_string_member_.value());
  if (usv_string_member_)
    dict->setUsvStringMember(usv_string_member_.value());
  if (string_sequence_member_)
    dict->setStringSequenceMember(string_sequence_member_.value());
  dict->setStringSequenceMemberWithDefault(
      string_sequence_member_with_default_);
  if (string_sequence_or_null_member_) {
    dict->setStringSequenceOrNullMember(
        string_sequence_or_null_member_.value());
  }
  if (enum_member_)
    dict->setEnumMember(enum_member_.value());
  dict->setEnumMemberWithDefault(enum_member_with_default_);
  if (enum_or_null_member_)
    dict->setEnumOrNullMember(enum_or_null_member_.value());
  if (element_member_)
    dict->setElementMember(element_member_);
  if (has_element_or_null_member_)
    dict->setElementOrNullMember(element_or_null_member_);
  dict->setObjectMember(object_member_);
  dict->setObjectOrNullMemberWithDefault(object_or_null_member_with_default_);
  if (!double_or_string_member_.IsNull())
    dict->setDoubleOrStringMember(double_or_string_member_);
  if (double_or_string_sequence_or_null_member_) {
    dict->setDoubleOrStringSequenceMember(
        *double_or_string_sequence_or_null_member_);
  }
  dict->setEventTargetOrNullMember(event_target_or_null_member_);
  dict->setInternalEnumOrInternalEnumSequenceMember(
      internal_enum_or_internal_enum_sequence_);
  dict->setAnyMember(any_member_);
  if (callback_function_member_)
    dict->setCallbackFunctionMember(callback_function_member_);
}

void DictionaryTest::GetDerivedInternals(InternalDictionaryDerived* dict) {
  GetInternals(dict);

  if (derived_string_member_)
    dict->setDerivedStringMember(derived_string_member_.value());
  dict->setDerivedStringMemberWithDefault(derived_string_member_with_default_);
  dict->setRequiredBooleanMember(required_boolean_member_);
}

void DictionaryTest::GetDerivedDerivedInternals(
    InternalDictionaryDerivedDerived* dict) {
  GetDerivedInternals(dict);

  if (derived_derived_string_member_)
    dict->setDerivedDerivedStringMember(derived_derived_string_member_.value());
}

void DictionaryTest::Trace(Visitor* visitor) const {
  visitor->Trace(element_member_);
  visitor->Trace(element_or_null_member_);
  visitor->Trace(object_member_);
  visitor->Trace(object_or_null_member_with_default_);
  visitor->Trace(double_or_string_sequence_or_null_member_);
  visitor->Trace(event_target_or_null_member_);
  visitor->Trace(any_member_);
  visitor->Trace(callback_function_member_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
