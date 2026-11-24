// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

class Element;
class StyleRequest;

// Optional backing object for a highlight pseudo-element that needs Element
// identity, for example to own CSS animations. Static highlights can resolve
// styles without an object. When an object exists, it resolves its own style;
// StyleHighlightData on the originating element's ComputedStyle still stores
// the highlight styles.
//
// The originating element owns the backing object through PseudoElementData.
// Highlight pseudo-elements never create layout objects.
class CORE_EXPORT HighlightPseudoElement final : public PseudoElement {
 public:
  HighlightPseudoElement(Element& origin,
                         PseudoId pseudo,
                         const AtomicString& argument);

  bool LayoutObjectIsNeeded(const DisplayStyle&) const override;

  // Returns an existing backing object, or null if absent or ineligible.
  static PseudoElement* FindTarget(const Element&, const StyleRequest&);

  // Returns an existing backing object, or creates and associates one.
  // Returns null if the request is ineligible for a backing object.
  static PseudoElement* EnsureTarget(Element&, const StyleRequest&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HIGHLIGHT_HIGHLIGHT_PSEUDO_ELEMENT_H_
