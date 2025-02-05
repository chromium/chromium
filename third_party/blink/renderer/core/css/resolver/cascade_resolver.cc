// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"

namespace blink {

bool CascadeResolver::IsLocked(const CSSProperty& property) const {
  return Find(property) != kNotFound;
}

bool CascadeResolver::IsLocked(const Attribute& attribute) const {
  return Find(attribute) != kNotFound;
}

bool CascadeResolver::IsLocked(const LocalVariable& local_variable) const {
  return Find(local_variable) != kNotFound;
}

bool CascadeResolver::IsLocked(const Function& function) const {
  return Find(function) != kNotFound;
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

bool CascadeResolver::DetectCycle(const CSSProperty& property) {
  return DetectCycle(Find(property));
}

bool CascadeResolver::DetectCycle(const Attribute& attribute) {
  return DetectCycle(Find(attribute));
}

bool CascadeResolver::DetectCycle(const LocalVariable& local_variable) {
  return DetectCycle(Find(local_variable));
}

bool CascadeResolver::DetectCycle(const Function& function) {
  return DetectCycle(Find(function));
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

wtf_size_t CascadeResolver::Find(const CSSProperty& property) const {
  wtf_size_t index = 0;
  for (CycleElem elem : stack_) {
    if (absl::holds_alternative<const CSSProperty*>(elem) &&
        absl::get<const CSSProperty*>(elem)->HasEqualCSSPropertyName(
            property)) {
      return index;
    }
    ++index;
  }
  return kNotFound;
}

namespace {

template <typename T>
wtf_size_t FindInternal(const CascadeResolver::CycleStack& stack,
                        const T& item) {
  wtf_size_t index = 0;
  for (CascadeResolver::CycleElem elem : stack) {
    if (absl::holds_alternative<T>(elem) && absl::get<T>(elem) == item) {
      return index;
    }
    ++index;
  }
  return kNotFound;
}

}  // namespace

wtf_size_t CascadeResolver::Find(const Attribute& attribute) const {
  return FindInternal(stack_, attribute);
}

wtf_size_t CascadeResolver::Find(const LocalVariable& local_variable) const {
  return FindInternal(stack_, local_variable);
}

wtf_size_t CascadeResolver::Find(const Function& function) const {
  return FindInternal(stack_, function);
}

CascadeResolver::AutoLock::AutoLock(const CSSProperty& property,
                                    CascadeResolver& resolver)
    : resolver_(resolver) {
  DCHECK(!resolver.IsLocked(property));
  resolver_.stack_.push_back(&property);
}

CascadeResolver::AutoLock::AutoLock(const Attribute& attribute,
                                    CascadeResolver& resolver)
    : resolver_(resolver) {
  DCHECK(!resolver.IsLocked(attribute));
  resolver_.stack_.push_back(attribute);
}

CascadeResolver::AutoLock::AutoLock(const LocalVariable& local_variable,
                                    CascadeResolver& resolver)
    : resolver_(resolver) {
  DCHECK(!resolver.IsLocked(local_variable));
  resolver_.stack_.push_back(local_variable);
}

CascadeResolver::AutoLock::AutoLock(const Function& function,
                                    CascadeResolver& resolver)
    : resolver_(resolver) {
  DCHECK(!resolver.IsLocked(function));
  resolver_.stack_.push_back(function);
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
