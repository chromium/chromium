// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/position_try_fallbacks.h"

namespace blink {

bool PositionTryFallback::operator==(const PositionTryFallback& other) const {
  return tactic_list_ == other.tactic_list_ &&
         base::ValuesEquivalent(position_try_name_, other.position_try_name_) &&
         position_area_ == other.position_area_;
}

bool PositionTryFallback::Matches(const PositionTryFallback& other) const {
  AtomicString name;
  AtomicString other_name;
  // TODO(crbug.com/417621241): Currently, TreeScope is ignored, which means
  // anchored(fallback: --foo) will match --foo from any tree, regardless of
  // where the @container rule or position-try-fallbacks property value
  // originates from.
  if (position_try_name_) {
    name = position_try_name_->GetName();
  }
  if (other.position_try_name_) {
    other_name = other.position_try_name_->GetName();
  }
  return tactic_list_ == other.tactic_list_ && name == other_name &&
         position_area_.Matches(other.position_area_);
}

void PositionTryFallback::Trace(Visitor* visitor) const {
  visitor->Trace(position_try_name_);
}

bool PositionTryFallbacks::operator==(const PositionTryFallbacks& other) const {
  return fallbacks_ == other.fallbacks_;
}

bool PositionTryFallbacks::HasPositionTryName(
    const HashSet<AtomicString>& names) const {
  for (const auto& fallback : fallbacks_) {
    if (const ScopedCSSName* scoped_name = fallback.GetPositionTryName()) {
      if (names.Contains(scoped_name->GetName())) {
        return true;
      }
    }
  }
  return false;
}

void PositionTryFallbacks::Trace(Visitor* visitor) const {
  visitor->Trace(fallbacks_);
}

}  // namespace blink
