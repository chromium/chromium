// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

void CascadeResolver::CycleNode::Trace(blink::Visitor* visitor) const {
  visitor->Trace(function);
}

bool CascadeResolver::AllowSubstitution(CSSVariableData* data) const {
  if (data && data->IsAnimationTainted() && stack_.size()) {
    const CSSProperty* property = CurrentProperty();
    if (IsA<CustomProperty>(*property)) {
      return true;
    }
    return !CSSAnimations::IsAnimationAffectingProperty(*property);
  }
  return true;
}

bool CascadeResolver::DetectCycle(wtf_size_t index) {
  if (index == kNotFound) {
    return false;
  }
  cycle_start_ = std::min(cycle_start_, index);
  cycle_end_ = stack_.size();
  DCHECK(InCycle());
  return true;
}

bool CascadeResolver::InCycle() const {
  return stack_.size() > cycle_start_ && stack_.size() <= cycle_end_;
}

wtf_size_t CascadeResolver::Find(const CycleNode& node) const {
  wtf_size_t index = 0;
  for (const CycleNode& stack_node : stack_) {
    if (stack_node == node) {
      return index;
    }
    ++index;
  }
  return kNotFound;
}

CascadeResolver::AutoLock::AutoLock(const CycleNode& node,
                                    CascadeResolver& resolver)
    : resolver_(resolver) {
  DCHECK(!resolver.IsLocked(node));
  resolver_.stack_.push_back(node);
}

CascadeResolver::AutoLock::~AutoLock() {
  resolver_.stack_.pop_back();
  if (resolver_.cycle_end_ != kNotFound) {
    resolver_.cycle_end_ =
        std::min(resolver_.cycle_end_, resolver_.stack_.size());
  }
  if (resolver_.cycle_end_ <= resolver_.cycle_start_) {
    resolver_.cycle_start_ = kNotFound;
    resolver_.cycle_end_ = kNotFound;
  }
}

}  // namespace blink
