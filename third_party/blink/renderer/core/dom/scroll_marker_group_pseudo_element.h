// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_MARKER_GROUP_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_MARKER_GROUP_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"
#include "third_party/blink/renderer/core/scroll/scroll_snapshot_client.h"

namespace blink {

// Represents ::scroll-marker-group pseudo element and manages
// implicit focus group, formed by ::scroll-marker pseudo elements.
// This focus group is needed to cycle through its element with
// arrow keys.
class ScrollMarkerGroupPseudoElement : public PseudoElement,
                                       public ScrollSnapshotClient {
 public:
  // pseudo_id is needed, as ::scroll-marker-group can be after or before.
  ScrollMarkerGroupPseudoElement(Element* originating_element,
                                 PseudoId pseudo_id);

  bool IsScrollMarkerGroupPseudoElement() const final { return true; }

  void AddToFocusGroup(ScrollMarkerPseudoElement& scroll_marker);
  void RemoveFromFocusGroup(const ScrollMarkerPseudoElement& scroll_marker);
  void ClearFocusGroup();
  ScrollMarkerPseudoElement* FindNextScrollMarker(const Element& current);
  ScrollMarkerPseudoElement* FindPreviousScrollMarker(const Element& current);
  const HeapVector<Member<ScrollMarkerPseudoElement>>& ScrollMarkers() {
    return focus_group_;
  }
  // Set selected scroll marker. Returns true if the selected marker changed.
  CORE_EXPORT bool SetSelected(ScrollMarkerPseudoElement& scroll_marker);
  ScrollMarkerPseudoElement* Selected() { return selected_marker_; }
  void ActivateNextScrollMarker();
  void ActivatePrevScrollMarker();

  void Dispose() final;
  void Trace(Visitor* v) const final;

  // ScrollSnapshotClient:
  void UpdateSnapshot() override;
  bool ValidateSnapshot() override;
  bool ShouldScheduleNextService() override;

 private:
  bool UpdateSelectedScrollMarker();

  void ActivateScrollMarker(ScrollMarkerPseudoElement* (
      ScrollMarkerGroupPseudoElement::*find_scroll_marker_func)(
      const Element&));

  // TODO(332396355): Add spec link, once it's created.
  HeapVector<Member<ScrollMarkerPseudoElement>> focus_group_;
  Member<ScrollMarkerPseudoElement> selected_marker_ = nullptr;
};

template <>
struct DowncastTraits<ScrollMarkerGroupPseudoElement> {
  static bool AllowFrom(const Node& node) {
    return node.IsScrollMarkerGroupPseudoElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SCROLL_MARKER_GROUP_PSEUDO_ELEMENT_H_
