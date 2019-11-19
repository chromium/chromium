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

class UnionTypesTest final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  UnionTypesTest() : attribute_type_(kSpecificTypeNone) {}
  ~UnionTypesTest() override = default;

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

 private:
  enum AttributeSpecificType {
    kSpecificTypeNone,
    kSpecificTypeDouble,
    kSpecificTypeString,
    kSpecificTypeStringSequence,
  };
  AttributeSpecificType attribute_type_;
  double attribute_double_;
  String attribute_string_;
  Vector<String> attribute_string_sequence_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_UNION_TYPES_TEST_H_
