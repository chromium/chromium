// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/successful_position_fallback.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/style/position_try_fallbacks.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CSSPropertyValueSet;
class LayoutObject;

class CORE_EXPORT OutOfFlowData final
    : public GarbageCollected<OutOfFlowData>,
      public ElementRareDataField {
 public:
  // For each layout of an OOF that ever had a successful try fallback, register
  // the current fallback. When ApplyPendingSuccessfulPositionFallback() is
  // called, update the last successful one.
  bool SetPendingSuccessfulPositionFallback(
      const PositionTryFallbacks* fallbacks,
      const CSSPropertyValueSet* try_set,
      const TryTacticList& try_tactics,
      std::optional<size_t> index);

  bool ClearPendingSuccessfulPositionFallback() {
    return SetPendingSuccessfulPositionFallback(nullptr, nullptr, kNoTryTactics,
                                                std::nullopt);
  }

  // At resize observer timing, update the last successful try fallback.
  // Returns true if last successful fallback was cleared.
  bool ApplyPendingSuccessfulPositionFallback(LayoutObject* layout_object);

  bool HasLastSuccessfulPositionFallback() const {
    return last_successful_position_fallback_.position_try_fallbacks_ !=
           nullptr;
  }

  // Clears the last successful position fallback if position-try-fallbacks
  // refer to any of the @position-try names passed in. Returns true if the last
  // successful fallbacks was cleared.
  bool InvalidatePositionTryNames(const HashSet<AtomicString>& try_names);

  const CSSPropertyValueSet* GetLastSuccessfulTrySet() const {
    return last_successful_position_fallback_.try_set_;
  }

  const TryTacticList& GetLastSuccessfulTryTactics() const {
    return last_successful_position_fallback_.try_tactics_;
  }

  std::optional<size_t> GetNewSuccessfulPositionFallbackIndex() const {
    if (new_successful_position_fallback_.index_ != std::nullopt) {
      return new_successful_position_fallback_.index_;
    }
    return last_successful_position_fallback_.index_;
  }

  void Trace(Visitor*) const override;

 private:
  void ClearLastSuccessfulPositionFallback();

  SuccessfulPositionFallback last_successful_position_fallback_;
  // If the previous layout had a successful position fallback, it is stored
  // here. Will be copied to the last_successful_position_fallback_ at next
  // resize observer update.
  SuccessfulPositionFallback new_successful_position_fallback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_
