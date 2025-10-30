// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/conditional_exp_node.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String ConditionalExpNode::Serialize() const {
  StringBuilder builder;
  SerializeTo(builder);
  return builder.ReleaseString();
}

const ConditionalExpNode* ConditionalExpNode::Not(
    const ConditionalExpNode* operand) {
  if (!operand) {
    return nullptr;
  }
  return MakeGarbageCollected<ConditionalExpNodeNot>(operand);
}

const ConditionalExpNode* ConditionalExpNode::Nested(
    const ConditionalExpNode* operand) {
  if (!operand) {
    return nullptr;
  }
  return MakeGarbageCollected<ConditionalExpNodeNested>(operand);
}

const ConditionalExpNode* ConditionalExpNode::Function(
    const ConditionalExpNode* operand,
    const AtomicString& name) {
  if (!operand) {
    return nullptr;
  }
  return MakeGarbageCollected<ConditionalExpNodeFunction>(operand, name);
}

const ConditionalExpNode* ConditionalExpNode::And(
    const ConditionalExpNode* left,
    const ConditionalExpNode* right) {
  if (!left || !right) {
    return nullptr;
  }
  return MakeGarbageCollected<ConditionalExpNodeAnd>(left, right);
}

const ConditionalExpNode* ConditionalExpNode::Or(
    const ConditionalExpNode* left,
    const ConditionalExpNode* right) {
  if (!left || !right) {
    return nullptr;
  }
  return MakeGarbageCollected<ConditionalExpNodeOr>(left, right);
}

void ConditionalExpNodeUnary::Trace(Visitor* v) const {
  ConditionalExpNode::Trace(v);
  v->Trace(operand_);
}

KleeneValue ConditionalExpNodeUnary::Evaluate(
    ConditionalLeafExpressionHandler& leaf_handler) const {
  return operand_->Evaluate(leaf_handler);
}

void ConditionalExpNodeUnary::CollectExpressions(
    HeapVector<MediaQueryExp>& expressions) const {
  return operand_->CollectExpressions(expressions);
}

ConditionalExpNode::FeatureFlags ConditionalExpNodeUnary::CollectFeatureFlags()
    const {
  return operand_->CollectFeatureFlags();
}

void ConditionalExpNodeCompound::Trace(Visitor* v) const {
  ConditionalExpNode::Trace(v);
  v->Trace(left_);
  v->Trace(right_);
}

void ConditionalExpNodeCompound::CollectExpressions(
    HeapVector<MediaQueryExp>& expressions) const {
  left_->CollectExpressions(expressions);
  right_->CollectExpressions(expressions);
}

ConditionalExpNode::FeatureFlags
ConditionalExpNodeCompound::CollectFeatureFlags() const {
  return left_->CollectFeatureFlags() | right_->CollectFeatureFlags();
}

KleeneValue ConditionalExpNodeAnd::Evaluate(
    ConditionalLeafExpressionHandler& leaf_handler) const {
  KleeneValue left_result = left_->Evaluate(leaf_handler);
  if (left_result == KleeneValue::kFalse) {
    /// Short-circuit.
    return left_result;
  }
  return KleeneAnd(left_result, right_->Evaluate(leaf_handler));
}

void ConditionalExpNodeAnd::SerializeTo(StringBuilder& builder) const {
  left_->SerializeTo(builder);
  builder.Append(" and ");
  right_->SerializeTo(builder);
}

KleeneValue ConditionalExpNodeOr::Evaluate(
    ConditionalLeafExpressionHandler& leaf_handler) const {
  KleeneValue left_result = left_->Evaluate(leaf_handler);
  if (left_result == KleeneValue::kTrue) {
    /// Short-circuit.
    return left_result;
  }
  return KleeneOr(left_result, right_->Evaluate(leaf_handler));
}

void ConditionalExpNodeOr::SerializeTo(StringBuilder& builder) const {
  left_->SerializeTo(builder);
  builder.Append(" or ");
  right_->SerializeTo(builder);
}

KleeneValue ConditionalExpNodeNot::Evaluate(
    ConditionalLeafExpressionHandler& leaf_handler) const {
  return KleeneNot(operand_->Evaluate(leaf_handler));
}

void ConditionalExpNodeNot::SerializeTo(StringBuilder& builder) const {
  builder.Append("not ");
  operand_->SerializeTo(builder);
}

void ConditionalExpNodeNested::SerializeTo(StringBuilder& builder) const {
  builder.Append("(");
  operand_->SerializeTo(builder);
  builder.Append(")");
}

void ConditionalExpNodeFunction::SerializeTo(StringBuilder& builder) const {
  builder.Append(name_);
  builder.Append("(");
  operand_->SerializeTo(builder);
  builder.Append(")");
}

ConditionalExpNode::FeatureFlags
ConditionalExpNodeFunction::CollectFeatureFlags() const {
  FeatureFlags flags = ConditionalExpNodeUnary::CollectFeatureFlags();
  if (name_ == AtomicString("style")) {
    flags |= kFeatureStyle;
  }
  return flags;
}

KleeneValue ConditionalExpNodeUnknown::Evaluate(
    ConditionalLeafExpressionHandler& leaf_handler) const {
  return KleeneValue::kUnknown;
}

void ConditionalExpNodeUnknown::SerializeTo(StringBuilder& builder) const {
  builder.Append(string_);
}

void ConditionalExpNodeUnknown::CollectExpressions(
    HeapVector<MediaQueryExp>&) const {}

ConditionalExpNode::FeatureFlags
ConditionalExpNodeUnknown::CollectFeatureFlags() const {
  return kFeatureUnknown;
}

}  // namespace blink
