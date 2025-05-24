// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/if_condition.h"

namespace blink {

const IfCondition* IfCondition::Not(const IfCondition* operand) {
  if (!operand) {
    return nullptr;
  }
  return MakeGarbageCollected<IfConditionNot>(operand);
}

const IfCondition* IfCondition::And(const IfCondition* left,
                                    const IfCondition* right) {
  if (!left || !right) {
    return nullptr;
  }
  return MakeGarbageCollected<IfConditionAnd>(left, right);
}

const IfCondition* IfCondition::Or(const IfCondition* left,
                                   const IfCondition* right) {
  if (!left || !right) {
    return nullptr;
  }
  return MakeGarbageCollected<IfConditionOr>(left, right);
}

void IfConditionNot::Trace(Visitor* visitor) const {
  visitor->Trace(operand_);
  IfCondition::Trace(visitor);
}

void IfConditionAnd::Trace(Visitor* visitor) const {
  visitor->Trace(left_);
  visitor->Trace(right_);
  IfCondition::Trace(visitor);
}

void IfConditionOr::Trace(Visitor* visitor) const {
  visitor->Trace(left_);
  visitor->Trace(right_);
  IfCondition::Trace(visitor);
}

void IfTestStyle::Trace(Visitor* visitor) const {
  visitor->Trace(style_test_);
  IfCondition::Trace(visitor);
}

void IfTestMedia::Trace(Visitor* visitor) const {
  visitor->Trace(media_test_);
  IfCondition::Trace(visitor);
}

void IfTestSupports::Trace(Visitor* visitor) const {
  IfCondition::Trace(visitor);
}

void IfConditionUnknown::Trace(Visitor* visitor) const {
  IfCondition::Trace(visitor);
}

void IfConditionElse::Trace(Visitor* visitor) const {
  IfCondition::Trace(visitor);
}

}  // namespace blink
