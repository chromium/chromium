// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_TRY_VALUE_FLIPS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_TRY_VALUE_FLIPS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/position_try_options.h"

namespace blink {

class CSSPropertyValueSet;

// A single position-try-option can specify a number of "flips" called
// try-tactics. This makes it easy for authors to try mirrored versions
// of manually specified positions.
//
// This class is responsible for carrying out those flips, or rather
// generating CSSPropertyValueSets which carry out those flips
// using CSSFlipRevertValues.
//
// https://drafts.csswg.org/css-anchor-position-1/#propdef-position-try-options
class CORE_EXPORT TryValueFlips {
  DISALLOW_NEW();

 public:
  // Generate a CSSPropertyValueSet containing CSSFlipRevertValue,
  // corresponding to the incoming TryTacticList.
  //
  // This will end up in OutOfFlowData::try_tactics_set_.
  const CSSPropertyValueSet* FlipSet(const TryTacticList&);

  // TODO(crbug.com/40279608): This will contain some cached
  // CSSPropertyValueSets in the future.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_TRY_VALUE_FLIPS_H_
