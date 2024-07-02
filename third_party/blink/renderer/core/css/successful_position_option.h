// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SUCCESSFUL_POSITION_OPTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SUCCESSFUL_POSITION_OPTION_H_

#include "third_party/blink/renderer/core/style/position_try_options.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CSSPropertyValueSet;

// When an anchored element is position using one of the position-try-options
// without overflowing, we need to keep track of it as the last successful
// option because if the same set of options in later layout cannot fit any of
// the options, we should fall back to the last successful one.
class SuccessfulPositionOption {
  DISALLOW_NEW();

 public:
  bool operator==(const SuccessfulPositionOption&) const;
  bool operator!=(const SuccessfulPositionOption& option) const {
    return !operator==(option);
  }
  bool IsEmpty() const { return position_try_options_ == nullptr; }
  void Clear();

  void Trace(Visitor*) const;

  // The computed value of position-try-options the sets below are based on
  Member<const PositionTryOptions> position_try_options_;
  // The try set used for the successful option
  Member<const CSSPropertyValueSet> try_set_;
  // The try tactics used for the successful option
  TryTacticList try_tactics_ = kNoTryTactics;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SUCCESSFUL_POSITION_OPTION_H_
