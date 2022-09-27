// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/dictionary_test.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_internal_dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internal_dictionary_derived.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internal_dictionary_derived_derived.h"

namespace blink {

DictionaryTest::DictionaryTest() = default;

DictionaryTest::~DictionaryTest() = default;

#define SAVE_DICT_MEMBER(camel, capital)                  \
  if (input_dictionary->has##capital()) {                 \
    dictionary_->set##capital(input_dictionary->camel()); \
  } else {                                                \
  }

void DictionaryTest::set(v8::Isolate* isolate,
                         const InternalDictionary* input_dictionary) {
  dictionary_ = InternalDictionaryDerivedDerived::Create(isolate);

  SAVE_DICT_MEMBER(longMember, LongMember);
  SAVE_DICT_MEMBER(longMemberWithClamp, LongMemberWithClamp);
  SAVE_DICT_MEMBER(longMemberWithEnforceRange, LongMemberWithEnforceRange);
  SAVE_DICT_MEMBER(longMemberWithDefault, LongMemberWithDefault);
  SAVE_DICT_MEMBER(longOrNullMember, LongOrNullMember);
  SAVE_DICT_MEMBER(longOrNullMemberWithDefault, LongOrNullMemberWithDefault);
  SAVE_DICT_MEMBER(booleanMember, BooleanMember);
  SAVE_DICT_MEMBER(doubleMember, DoubleMember);
  SAVE_DICT_MEMBER(unrestrictedDoubleMember, UnrestrictedDoubleMember);
  SAVE_DICT_MEMBER(stringMember, StringMember);
  SAVE_DICT_MEMBER(stringMemberWithDefault, StringMemberWithDefault);
  SAVE_DICT_MEMBER(byteStringMember, ByteStringMember);
  SAVE_DICT_MEMBER(usvStringMember, UsvStringMember);
  SAVE_DICT_MEMBER(stringSequenceMember, StringSequenceMember);
  SAVE_DICT_MEMBER(stringSequenceMemberWithDefault,
                   StringSequenceMemberWithDefault);
  SAVE_DICT_MEMBER(stringSequenceOrNullMember, StringSequenceOrNullMember);
  SAVE_DICT_MEMBER(enumMember, EnumMember);
  SAVE_DICT_MEMBER(enumMemberWithDefault, EnumMemberWithDefault);
  SAVE_DICT_MEMBER(enumOrNullMember, EnumOrNullMember);
  SAVE_DICT_MEMBER(elementMember, ElementMember);
  SAVE_DICT_MEMBER(elementOrNullMember, ElementOrNullMember);
  SAVE_DICT_MEMBER(objectMember, ObjectMember);
  SAVE_DICT_MEMBER(objectOrNullMemberWithDefault,
                   ObjectOrNullMemberWithDefault);
  SAVE_DICT_MEMBER(doubleOrStringMember, DoubleOrStringMember);
  SAVE_DICT_MEMBER(doubleOrStringSequenceMember, DoubleOrStringSequenceMember);
  SAVE_DICT_MEMBER(eventTargetOrNullMember, EventTargetOrNullMember);
  SAVE_DICT_MEMBER(internalEnumOrInternalEnumSequenceMember,
                   InternalEnumOrInternalEnumSequenceMember);
  SAVE_DICT_MEMBER(anyMember, AnyMember);
  SAVE_DICT_MEMBER(callbackFunctionMember, CallbackFunctionMember);
}

InternalDictionary* DictionaryTest::get(v8::Isolate* isolate) {
  InternalDictionary* dictionary = InternalDictionary::Create(isolate);
  RestoreInternalDictionary(dictionary);
  return dictionary;
}

void DictionaryTest::setDerived(
    v8::Isolate* isolate,
    const InternalDictionaryDerived* input_dictionary) {
  set(isolate, input_dictionary);

  SAVE_DICT_MEMBER(derivedStringMember, DerivedStringMember);
  SAVE_DICT_MEMBER(derivedStringMemberWithDefault,
                   DerivedStringMemberWithDefault);
  SAVE_DICT_MEMBER(requiredBooleanMember, RequiredBooleanMember);
}

InternalDictionaryDerived* DictionaryTest::getDerived(v8::Isolate* isolate) {
  InternalDictionaryDerived* dictionary =
      InternalDictionaryDerived::Create(isolate);
  RestoreInternalDictionaryDerived(dictionary);
  return dictionary;
}

void DictionaryTest::setDerivedDerived(
    v8::Isolate* isolate,
    const InternalDictionaryDerivedDerived* input_dictionary) {
  setDerived(isolate, input_dictionary);

  SAVE_DICT_MEMBER(derivedDerivedStringMember, DerivedDerivedStringMember);
}

InternalDictionaryDerivedDerived* DictionaryTest::getDerivedDerived(
    v8::Isolate* isolate) {
  InternalDictionaryDerivedDerived* dictionary =
      InternalDictionaryDerivedDerived::Create(isolate);
  RestoreInternalDictionaryDerivedDerived(dictionary);
  return dictionary;
}

#undef SAVE_DICT_MEMBER

void DictionaryTest::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(dictionary_);
}

#define RESTORE_DICT_MEMBER(camel, capital)                \
  if (dictionary_->has##capital()) {                       \
    output_dictionary->set##capital(dictionary_->camel()); \
  } else {                                                 \
  }

void DictionaryTest::RestoreInternalDictionary(
    InternalDictionary* output_dictionary) {
  RESTORE_DICT_MEMBER(longMember, LongMember);
  RESTORE_DICT_MEMBER(longMemberWithClamp, LongMemberWithClamp);
  RESTORE_DICT_MEMBER(longMemberWithEnforceRange, LongMemberWithEnforceRange);
  RESTORE_DICT_MEMBER(longMemberWithDefault, LongMemberWithDefault);
  RESTORE_DICT_MEMBER(longOrNullMember, LongOrNullMember);
  RESTORE_DICT_MEMBER(longOrNullMemberWithDefault, LongOrNullMemberWithDefault);
  RESTORE_DICT_MEMBER(booleanMember, BooleanMember);
  RESTORE_DICT_MEMBER(doubleMember, DoubleMember);
  RESTORE_DICT_MEMBER(unrestrictedDoubleMember, UnrestrictedDoubleMember);
  RESTORE_DICT_MEMBER(stringMember, StringMember);
  RESTORE_DICT_MEMBER(stringMemberWithDefault, StringMemberWithDefault);
  RESTORE_DICT_MEMBER(byteStringMember, ByteStringMember);
  RESTORE_DICT_MEMBER(usvStringMember, UsvStringMember);
  RESTORE_DICT_MEMBER(stringSequenceMember, StringSequenceMember);
  RESTORE_DICT_MEMBER(stringSequenceMemberWithDefault,
                      StringSequenceMemberWithDefault);
  RESTORE_DICT_MEMBER(stringSequenceOrNullMember, StringSequenceOrNullMember);
  RESTORE_DICT_MEMBER(enumMember, EnumMember);
  RESTORE_DICT_MEMBER(enumMemberWithDefault, EnumMemberWithDefault);
  RESTORE_DICT_MEMBER(enumOrNullMember, EnumOrNullMember);
  RESTORE_DICT_MEMBER(elementMember, ElementMember);
  RESTORE_DICT_MEMBER(elementOrNullMember, ElementOrNullMember);
  RESTORE_DICT_MEMBER(objectMember, ObjectMember);
  RESTORE_DICT_MEMBER(objectOrNullMemberWithDefault,
                      ObjectOrNullMemberWithDefault);
  RESTORE_DICT_MEMBER(doubleOrStringMember, DoubleOrStringMember);
  RESTORE_DICT_MEMBER(doubleOrStringSequenceMember,
                      DoubleOrStringSequenceMember);
  RESTORE_DICT_MEMBER(eventTargetOrNullMember, EventTargetOrNullMember);
  RESTORE_DICT_MEMBER(internalEnumOrInternalEnumSequenceMember,
                      InternalEnumOrInternalEnumSequenceMember);
  RESTORE_DICT_MEMBER(anyMember, AnyMember);
  RESTORE_DICT_MEMBER(callbackFunctionMember, CallbackFunctionMember);
}

void DictionaryTest::RestoreInternalDictionaryDerived(
    InternalDictionaryDerived* output_dictionary) {
  RestoreInternalDictionary(output_dictionary);

  RESTORE_DICT_MEMBER(derivedStringMember, DerivedStringMember);
  RESTORE_DICT_MEMBER(derivedStringMemberWithDefault,
                      DerivedStringMemberWithDefault);
  RESTORE_DICT_MEMBER(requiredBooleanMember, RequiredBooleanMember);
}

void DictionaryTest::RestoreInternalDictionaryDerivedDerived(
    InternalDictionaryDerivedDerived* output_dictionary) {
  RestoreInternalDictionaryDerived(output_dictionary);

  RESTORE_DICT_MEMBER(derivedDerivedStringMember, DerivedDerivedStringMember);
}

#undef RESTORE_DICT_MEMBER

}  // namespace blink
