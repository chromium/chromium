// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/successful_position_option.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/style/position_try_options.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CSSPropertyValueSet;
class LayoutObject;

class CORE_EXPORT OutOfFlowData final
    : public GarbageCollected<OutOfFlowData>,
      public ElementRareDataField {
 public:
  // For each layout of an OOF that ever had a successful try option, register
  // the current option. When ApplyPendingSuccessfulPositionOption() is called,
  // update the last successful one.
  bool SetPendingSuccessfulPositionOption(const PositionTryOptions* options,
                                          const CSSPropertyValueSet* try_set,
                                          const TryTacticList& try_tactics);

  bool ClearPendingSuccessfulPositionOption() {
    return SetPendingSuccessfulPositionOption(nullptr, nullptr, kNoTryTactics);
  }

  // At resize observer timing, update the last successful try option.
  // Returns true if last successful option was cleared.
  bool ApplyPendingSuccessfulPositionOption(LayoutObject* layout_object);

  bool HasLastSuccessfulPositionOption() const {
    return last_successful_position_option_.position_try_options_ != nullptr;
  }

  // Clears the last successful position option if position-try-options refer
  // to any of the @position-try names passed in. Returns true if the last
  // successful options was cleared.
  bool InvalidatePositionTryNames(const HashSet<AtomicString>& try_names);

  const CSSPropertyValueSet* GetLastSuccessfulTrySet() const {
    return last_successful_position_option_.try_set_;
  }

  const TryTacticList& GetLastSuccessfulTryTactics() const {
    return last_successful_position_option_.try_tactics_;
  }

  void Trace(Visitor*) const override;

 private:
  void ClearLastSuccessfulPositionOption();

  SuccessfulPositionOption last_successful_position_option_;
  // If the previous layout had a successful position option, it is stored here.
  // Will be copied to the last_successful_position_option_ at next resize
  // observer update.
  SuccessfulPositionOption new_successful_position_option_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_
