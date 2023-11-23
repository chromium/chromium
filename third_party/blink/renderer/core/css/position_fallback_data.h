// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_POSITION_FALLBACK_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_POSITION_FALLBACK_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CSSPropertyValueSet;

class CORE_EXPORT PositionFallbackData final
    : public GarbageCollected<PositionFallbackData>,
      public ElementRareDataField {
 public:
  // Speculative @try styling: the last @try rule chosen by
  // layout/OOFCandidateStyleIterator is stored on the element, and subsequent
  // style resolutions will continue to use this set until told otherwise by
  // OOFCandidateStyleIterator, or until the element stops being
  // out-of-flow-positioned (see StyleCascade::TreatAsRevertLayer).
  void SetTryPropertyValueSet(const CSSPropertyValueSet* set) {
    try_set_ = set;
  }

  const CSSPropertyValueSet* GetTryPropertyValueSet() const { return try_set_; }

  void Trace(Visitor*) const override;

 private:
  // Contains the declaration block of a @try rule.
  //
  // During calls to StyleResolver::ResolveStyle, the CSSPropertyValueSet
  // present here will be added to the cascade in the author origin
  // with CascadePriority::is_fallback_style=true.
  //
  // See also StyleEngine::UpdateStyleForPositionFallback,
  // which sets this value.
  Member<const CSSPropertyValueSet> try_set_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_POSITION_FALLBACK_DATA_H_
