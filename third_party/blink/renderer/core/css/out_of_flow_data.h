// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/anchor_results.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CSSPropertyValueSet;

// Contains data related to interleaved style updates from
// OOF-layout.
//
// See UpdateStyleForOutOfFlow.
class CORE_EXPORT OutOfFlowData final
    : public GarbageCollected<OutOfFlowData>,
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

  AnchorResults& GetAnchorResults() { return anchor_results_; }
  const AnchorResults& GetAnchorResults() const { return anchor_results_; }

  void Trace(Visitor*) const override;

 private:
  // Contains the declaration block of a @try rule.
  //
  // During calls to StyleResolver::ResolveStyle, the CSSPropertyValueSet
  // present here will be added to the cascade in the author origin
  // with CascadePriority::is_fallback_style=true.
  //
  // See also StyleEngine::UpdateStyleForOutOfFlow,
  // which sets this value.
  Member<const CSSPropertyValueSet> try_set_;

  // During interleaved style updates for out-of-flow elements,
  // the result of any anchor() or anchor-size() functions are stored here,
  // by an instance of ResultCachingAnchorEvaluator. The AnchorResults object
  // is then used directly as an AnchorEvaluator during regular
  // (non-interleaved) style recalcs.
  AnchorResults anchor_results_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_
