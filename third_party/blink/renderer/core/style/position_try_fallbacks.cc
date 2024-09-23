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
