// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/union_types_test.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_internalenum.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_nodelist.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
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
  NOTREACHED();
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

  NOTREACHED();
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

  NOTREACHED();
  return String();
}

String UnionTypesTest::doubleOrStringSequenceArg(
    HeapVector<Member<V8UnionDoubleOrString>>& sequence) {
  StringBuilder builder;
  for (auto& double_or_string : sequence) {
    DCHECK(double_or_string);
    if (!builder.IsEmpty())
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

  NOTREACHED();
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

  NOTREACHED();
  return String();
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
void UnionTypesTest::doubleOrStringOrStringSequenceAttribute(
    DoubleOrStringOrStringSequence& double_or_string_or_string_sequence) {
  switch (attribute_type_) {
    case kSpecificTypeNone:
      // Default value is zero (of double).
      double_or_string_or_string_sequence.SetDouble(0);
      break;
    case kSpecificTypeDouble:
      double_or_string_or_string_sequence.SetDouble(attribute_double_);
      break;
    case kSpecificTypeString:
      double_or_string_or_string_sequence.SetString(attribute_string_);
      break;
    case kSpecificTypeStringSequence:
      double_or_string_or_string_sequence.SetStringSequence(
          attribute_string_sequence_);
      break;
    default:
      NOTREACHED();
  }
}

void UnionTypesTest::setDoubleOrStringOrStringSequenceAttribute(
    const DoubleOrStringOrStringSequence& double_or_string_or_string_sequence) {
  if (double_or_string_or_string_sequence.IsDouble()) {
    attribute_double_ = double_or_string_or_string_sequence.GetAsDouble();
    attribute_type_ = kSpecificTypeDouble;
  } else if (double_or_string_or_string_sequence.IsString()) {
    attribute_string_ = double_or_string_or_string_sequence.GetAsString();
    attribute_type_ = kSpecificTypeString;
  } else if (double_or_string_or_string_sequence.IsStringSequence()) {
    attribute_string_sequence_ =
        double_or_string_or_string_sequence.GetAsStringSequence();
    attribute_type_ = kSpecificTypeStringSequence;
  } else {
    NOTREACHED();
  }
}

String UnionTypesTest::doubleOrStringArg(DoubleOrString& double_or_string) {
  if (double_or_string.IsNull())
    return "null is passed";
  if (double_or_string.IsDouble()) {
    return "double is passed: " +
           String::NumberToStringECMAScript(double_or_string.GetAsDouble());
  }
  if (double_or_string.IsString())
    return "string is passed: " + double_or_string.GetAsString();
  NOTREACHED();
  return String();
}

String UnionTypesTest::doubleOrInternalEnumArg(
    DoubleOrInternalEnum& double_or_internal_enum) {
  if (double_or_internal_enum.IsDouble()) {
    return "double is passed: " + String::NumberToStringECMAScript(
                                      double_or_internal_enum.GetAsDouble());
  }
  if (double_or_internal_enum.IsInternalEnum()) {
    return "InternalEnum is passed: " +
           double_or_internal_enum.GetAsInternalEnum();
  }
  NOTREACHED();
  return String();
}

String UnionTypesTest::doubleOrStringSequenceArg(
    HeapVector<DoubleOrString>& sequence) {
  if (!sequence.size())
    return "";

  StringBuilder builder;
  for (DoubleOrString& double_or_string : sequence) {
    DCHECK(!double_or_string.IsNull());
    if (double_or_string.IsDouble()) {
      builder.Append("double: ");
      builder.Append(
          String::NumberToStringECMAScript(double_or_string.GetAsDouble()));
    } else if (double_or_string.IsString()) {
      builder.Append("string: ");
      builder.Append(double_or_string.GetAsString());
    } else {
      NOTREACHED();
    }
    builder.Append(", ");
  }
  return builder.Substring(0, builder.length() - 2);
}

String UnionTypesTest::nodeListOrElementArg(
    NodeListOrElement& node_list_or_element) {
  DCHECK(!node_list_or_element.IsNull());
  return nodeListOrElementOrNullArg(node_list_or_element);
}

String UnionTypesTest::nodeListOrElementOrNullArg(
    NodeListOrElement& node_list_or_element_or_null) {
  if (node_list_or_element_or_null.IsNull())
    return "null or undefined is passed";
  if (node_list_or_element_or_null.IsNodeList())
    return "nodelist is passed";
  if (node_list_or_element_or_null.IsElement())
    return "element is passed";
  NOTREACHED();
  return String();
}

String UnionTypesTest::doubleOrStringOrStringSequenceArg(
    const DoubleOrStringOrStringSequence& double_or_string_or_string_sequence) {
  if (double_or_string_or_string_sequence.IsNull())
    return "null";

  if (double_or_string_or_string_sequence.IsDouble()) {
    return "double: " + String::NumberToStringECMAScript(
                            double_or_string_or_string_sequence.GetAsDouble());
  }

  if (double_or_string_or_string_sequence.IsString())
    return "string: " + double_or_string_or_string_sequence.GetAsString();

  DCHECK(double_or_string_or_string_sequence.IsStringSequence());
  const Vector<String>& sequence =
      double_or_string_or_string_sequence.GetAsStringSequence();
  if (!sequence.size())
    return "sequence: []";
  StringBuilder builder;
  builder.Append("sequence: [");
  for (const String& item : sequence) {
    DCHECK(!item.IsNull());
    builder.Append(item);
    builder.Append(", ");
  }
  return builder.Substring(0, builder.length() - 2) + "]";
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

}  // namespace blink
