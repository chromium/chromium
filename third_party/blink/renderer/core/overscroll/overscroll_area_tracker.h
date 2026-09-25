// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_AREA_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_AREA_TRACKER_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_rare_data_field.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ComputedStyle;
class ComputedStyleBuilder;
class Element;

// Tracks overscroll area elements associated with an overscroll container
// element. It maintains the DOM-sorted order of areas (which corresponds to
// visual stacking order where earlier DOM siblings are on top of later
// siblings), manages open/close actions, and provides queries for determining
// inertness of areas, container content, and command invokers.
class CORE_EXPORT OverscrollAreaTracker
    : public GarbageCollected<OverscrollAreaTracker>,
      public NodeRareDataField {
 public:
  explicit OverscrollAreaTracker(Element*);

  void AddOverscroll(Element*);
  void RemoveOverscroll(Element*);
  void RemoveAllOverscroll();

  void ToggleArea(Element* overscroll_area);
  void OpenArea(Element* overscroll_area);
  void CloseArea(Element* overscroll_area);
  void CloseAllAreas();

  const VectorOf<Element>& DOMSortedElements();

  static bool IsValidOverscrollArea(Element& element,
                                    const ComputedStyleBuilder& style_builder,
                                    const ComputedStyle* parent_style);
  static bool IsValidOverscrollArea(Element& element,
                                    const ComputedStyle* style,
                                    const ComputedStyle* parent_style);

  static void AdjustInertness(const Element& element,
                              bool is_overscroll_area,
                              const ComputedStyle& parent_style,
                              std::optional<bool>& html_inert,
                              bool& can_escape_overscroll_inertness);

  // Returns true if there is an open overscroll area above |area| in the visual
  // stacking order (i.e. preceding |area| in DOM order). If so, |area| is
  // covered by the open area above it and should be inert.
  bool HasOpenAreaAbove(const Element* area);

  // Returns true if any overscroll area in this container is currently open.
  bool HasAnyOpenArea() const;

  // Returns the overscroll area element that contains |element| (or |element|
  // itself if it is an overscroll area), or nullptr if |element| is not part of
  // an overscroll area.
  const Element* ContainingOverscrollArea(const Element* element) const;

  // Returns true if inertness should be removed for |invoker| (with
  // command="toggle-overscroll" and commandfor targeting |target|).
  // Specifically, a toggle invoker that is part of a closed overscroll area
  // escapes inertness (e.g. a handle or tab peaking out when the area is
  // closed) so that it can be interacted with to open the area, provided it
  // is not covered by an open area above it in visual stacking order.
  // Note that returning false does not mean |invoker| will be inert; it only
  // means inertness is not explicitly removed by this rule (so normal inertness
  // inheritance applies).
  bool ShouldRemoveInertness(const Element* invoker, const Element* target);

  void Trace(Visitor*) const override;

 private:
  friend class OverscrollAreaTrackerTest;

  Member<Element> container_;

  VectorOf<Element> overscroll_members_;
  bool needs_dom_sort_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_AREA_TRACKER_H_
