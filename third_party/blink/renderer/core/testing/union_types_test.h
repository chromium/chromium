// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_UNION_TYPES_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_UNION_TYPES_TEST_H_

#include "third_party/blink/renderer/bindings/core/v8/double_or_internal_enum.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_string.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_string_or_string_sequence.h"
#include "third_party/blink/renderer/bindings/core/v8/node_list_or_element.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class V8UnionDoubleOrInternalEnum;
class V8UnionDoubleOrString;
class V8UnionDoubleOrStringOrStringSequence;
class V8UnionElementOrNodeList;

class UnionTypesTest final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  UnionTypesTest() = default;
  ~UnionTypesTest() override = default;

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  V8UnionDoubleOrStringOrStringSequence*
  doubleOrStringOrStringSequenceAttribute() const;
  void setDoubleOrStringOrStringSequenceAttribute(
      const V8UnionDoubleOrStringOrStringSequence* value);

  String doubleOrStringArg(V8UnionDoubleOrString* arg);
  String doubleOrInternalEnumArg(V8UnionDoubleOrInternalEnum* arg);
  String doubleOrStringSequenceArg(
      HeapVector<Member<V8UnionDoubleOrString>>& sequence);

  String nodeListOrElementArg(const V8UnionElementOrNodeList* arg);
  String nodeListOrElementOrNullArg(const V8UnionElementOrNodeList* arg);

  String doubleOrStringOrStringSequenceArg(
      const V8UnionDoubleOrStringOrStringSequence* arg);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  void doubleOrStringOrStringSequenceAttribute(DoubleOrStringOrStringSequence&);
  void setDoubleOrStringOrStringSequenceAttribute(
      const DoubleOrStringOrStringSequence&);

  String doubleOrStringArg(DoubleOrString&);
  String doubleOrInternalEnumArg(DoubleOrInternalEnum&);
  String doubleOrStringSequenceArg(HeapVector<DoubleOrString>&);

  String nodeListOrElementArg(NodeListOrElement&);
  String nodeListOrElementOrNullArg(NodeListOrElement&);

  String doubleOrStringOrStringSequenceArg(
      const DoubleOrStringOrStringSequence&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

 private:
  enum AttributeSpecificType {
#if !defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    kSpecificTypeNone,
#endif  // !defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    kSpecificTypeDouble,
    kSpecificTypeString,
    kSpecificTypeStringSequence,
  };
  AttributeSpecificType attribute_type_ = kSpecificTypeDouble;
  double attribute_double_ = 0.;
  String attribute_string_;
  Vector<String> attribute_string_sequence_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_UNION_TYPES_TEST_H_
