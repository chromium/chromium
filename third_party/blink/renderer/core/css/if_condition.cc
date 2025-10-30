// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/if_condition.h"

namespace blink {

void IfTestMedia::Trace(Visitor* visitor) const {
  visitor->Trace(media_test_);
  ConditionalExpNode::Trace(visitor);
}

KleeneValue IfTestMedia::Evaluate(
    ConditionalLeafExpressionHandler& leaf_handler) const {
  return leaf_handler.EvaluateMediaQuerySet(*media_test_);
}

void IfTestMedia::SerializeTo(StringBuilder& builder) const {
  // No serialization needed inside if().
  NOTREACHED();
}

void IfTestSupports::Trace(Visitor* visitor) const {
  ConditionalExpNode::Trace(visitor);
}

KleeneValue IfTestSupports::Evaluate(ConditionalLeafExpressionHandler&) const {
  return result_ ? KleeneValue::kTrue : KleeneValue::kFalse;
}

void IfTestSupports::SerializeTo(StringBuilder&) const {
  // No serialization needed inside if().
  NOTREACHED();
}

void IfConditionElse::Trace(Visitor* visitor) const {
  ConditionalExpNode::Trace(visitor);
}

KleeneValue IfConditionElse::Evaluate(ConditionalLeafExpressionHandler&) const {
  return KleeneValue::kTrue;
}

void IfConditionElse::SerializeTo(StringBuilder&) const {
  // No serialization needed inside if().
  NOTREACHED();
}

}  // namespace blink
