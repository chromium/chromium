// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/out_of_flow_data.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

void OutOfFlowData::Trace(Visitor* visitor) const {
  ElementRareDataField::Trace(visitor);
  visitor->Trace(last_successful_position_option_);
  visitor->Trace(new_successful_position_option_);
}

bool OutOfFlowData::SetPendingSuccessfulPositionOption(
    const PositionTryOptions* options,
    const CSSPropertyValueSet* try_set,
    const TryTacticList& try_tactics) {
  new_successful_position_option_.position_try_options_ = options;
  new_successful_position_option_.try_set_ = try_set;
  new_successful_position_option_.try_tactics_ = try_tactics;
  return last_successful_position_option_ != new_successful_position_option_;
}

bool OutOfFlowData::ApplyPendingSuccessfulPositionOption(
    LayoutObject* layout_object) {
  if (!new_successful_position_option_.IsEmpty()) {
    last_successful_position_option_ = new_successful_position_option_;
    new_successful_position_option_.Clear();
    // Last attempt resulted in new successful option, which means the anchored
    // element already has the correct layout.
    return false;
  }
  if (!layout_object || !layout_object->IsOutOfFlowPositioned()) {
    // Element no longer renders as an OOF positioned. Clear last successful
    // position option, but no need for another layout since the previous
    // lifecycle update would not have applied a successful option.
    last_successful_position_option_.Clear();
    return false;
  }
  if (!last_successful_position_option_.IsEmpty() &&
      !base::ValuesEquivalent(
          last_successful_position_option_.position_try_options_.Get(),
          layout_object->StyleRef().GetPositionTryOptions().Get())) {
    // position-try-options changed which means the last successful option is
    // no longer valid. Clear and return true for a re-layout.
    last_successful_position_option_.Clear();
    return true;
  }
  return false;
}

void OutOfFlowData::ClearLastSuccessfulPositionOption() {
  last_successful_position_option_.Clear();
  new_successful_position_option_.Clear();
}

bool OutOfFlowData::InvalidatePositionTryNames(
    const HashSet<AtomicString>& try_names) {
  if (HasLastSuccessfulPositionOption()) {
    if (last_successful_position_option_.position_try_options_
            ->HasPositionTryName(try_names)) {
      ClearLastSuccessfulPositionOption();
      return true;
    }
  }
  return false;
}

}  // namespace blink
