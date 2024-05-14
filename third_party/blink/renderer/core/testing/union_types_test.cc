// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/union_types_test.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_internalenum.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_nodelist.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

V8UnionDoubleOrStringOrStringSequence*
UnionTypesTest::doubleOrStringOrStringSequenceAttribute() const {
  switch (attribute_type_) {
    case kSpecificTypeDouble:
      return MakeGarbageCollected<V8UnionDoubleOrStringOrStringSequence>(
          attribute_double_);
    case kSpecificTypeString:
      return MakeGarbageCollected<V8UnionDoubleOrStringOrStringSequence>(
          attribute_string_);
    case kSpecificTypeStringSequence:
      return MakeGarbageCollected<V8UnionDoubleOrStringOrStringSequence>(
          attribute_string_sequence_);
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void UnionTypesTest::setDoubleOrStringOrStringSequenceAttribute(
    const V8UnionDoubleOrStringOrStringSequence* value) {
  DCHECK(value);

  switch (value->GetContentType()) {
    case V8UnionDoubleOrStringOrStringSequence::ContentType::kDouble:
      attribute_double_ = value->GetAsDouble();
      attribute_type_ = kSpecificTypeDouble;
      break;
    case V8UnionDoubleOrStringOrStringSequence::ContentType::kString:
      attribute_string_ = value->GetAsString();
      attribute_type_ = kSpecificTypeString;
      break;
    case V8UnionDoubleOrStringOrStringSequence::ContentType::kStringSequence:
      attribute_string_sequence_ = value->GetAsStringSequence();
      attribute_type_ = kSpecificTypeStringSequence;
      break;
  }
}

String UnionTypesTest::doubleOrStringArg(V8UnionDoubleOrString* arg) {
  if (!arg)
    return "null is passed";

  switch (arg->GetContentType()) {
    case V8UnionDoubleOrString::ContentType::kDouble:
      return "double is passed: " +
             String::NumberToStringECMAScript(arg->GetAsDouble());
    case V8UnionDoubleOrString::ContentType::kString:
      return "string is passed: " + arg->GetAsString();
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

String UnionTypesTest::doubleOrInternalEnumArg(
    V8UnionDoubleOrInternalEnum* arg) {
  DCHECK(arg);

  switch (arg->GetContentType()) {
    case V8UnionDoubleOrInternalEnum::ContentType::kDouble:
      return "double is passed: " +
             String::NumberToStringECMAScript(arg->GetAsDouble());
    case V8UnionDoubleOrInternalEnum::ContentType::kInternalEnum:
      return "InternalEnum is passed: " + arg->GetAsInternalEnum().AsString();
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

String UnionTypesTest::doubleOrStringSequenceArg(
    HeapVector<Member<V8UnionDoubleOrString>>& sequence) {
  StringBuilder builder;
  for (auto& double_or_string : sequence) {
    DCHECK(double_or_string);
    if (!builder.empty())
      builder.Append(", ");
    switch (double_or_string->GetContentType()) {
      case V8UnionDoubleOrString::ContentType::kDouble:
        builder.Append("double: ");
        builder.Append(
            String::NumberToStringECMAScript(double_or_string->GetAsDouble()));
        break;
      case V8UnionDoubleOrString::ContentType::kString:
        builder.Append("string: ");
        builder.Append(double_or_string->GetAsString());
        break;
    }
  }
  return builder.ToString();
}

String UnionTypesTest::nodeListOrElementArg(
    const V8UnionElementOrNodeList* arg) {
  DCHECK(arg);
  return nodeListOrElementOrNullArg(arg);
}

String UnionTypesTest::nodeListOrElementOrNullArg(
    const V8UnionElementOrNodeList* arg) {
  if (!arg)
    return "null or undefined is passed";

  switch (arg->GetContentType()) {
    case V8UnionElementOrNodeList::ContentType::kElement:
      return "element is passed";
    case V8UnionElementOrNodeList::ContentType::kNodeList:
      return "nodelist is passed";
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

String UnionTypesTest::doubleOrStringOrStringSequenceArg(
    const V8UnionDoubleOrStringOrStringSequence* arg) {
  if (!arg)
    return "null";

  switch (arg->GetContentType()) {
    case V8UnionDoubleOrStringOrStringSequence::ContentType::kDouble:
      return "double: " + String::NumberToStringECMAScript(arg->GetAsDouble());
    case V8UnionDoubleOrStringOrStringSequence::ContentType::kString:
      return "string: " + arg->GetAsString();
    case V8UnionDoubleOrStringOrStringSequence::ContentType::kStringSequence: {
      StringBuilder builder;
      builder.Append("sequence: [");
      bool is_first = true;
      for (const String& item : arg->GetAsStringSequence()) {
        DCHECK(!item.IsNull());
        if (is_first)
          is_first = false;
        else
          builder.Append(", ");
        builder.Append(item);
      }
      builder.Append("]");
      return builder.ToString();
    }
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

}  // namespace blink
