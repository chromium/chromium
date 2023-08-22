// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_PSEUDO_ELEMENT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_PSEUDO_ELEMENT_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {
class ViewTransitionStyleTracker;

class CORE_EXPORT ViewTransitionPseudoElementBase : public PseudoElement {
 public:
  ViewTransitionPseudoElementBase(
      Element* parent,
      PseudoId,
      const AtomicString& view_transition_name,
      const ViewTransitionStyleTracker* style_tracker);
  ~ViewTransitionPseudoElementBase() override = default;

  bool CanGeneratePseudoElement(PseudoId) const override;
  const ComputedStyle* CustomStyleForLayoutObject(
      const StyleRecalcContext&) override;
  void Trace(Visitor* visitor) const override;

 protected:
  Member<const ViewTransitionStyleTracker> style_tracker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_PSEUDO_ELEMENT_BASE_H_
