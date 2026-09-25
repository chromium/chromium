// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

PaintLayerScrollableArea* GetScrollableAreaFor(Element* overscroll_area) {
  auto* overscroll_area_parent =
      overscroll_area->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  if (!overscroll_area_parent) {
    return nullptr;
  }
  auto* overscroll_area_object =
      DynamicTo<LayoutBox>(overscroll_area_parent->GetLayoutObject());
  if (!overscroll_area_object) {
    return nullptr;
  }
  return DynamicTo<PaintLayerScrollableArea>(
      overscroll_area_object->GetScrollableArea());
}

void ScrollTo(PaintLayerScrollableArea* scrollable_area, ScrollOffset offset) {
  // Snap on both axes to ensure target snap area element IDs are updated on
  // both axes even for single-axis scrolls or no-op offsets.
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(
          scrollable_area->ScrollOffsetToPosition(offset),
          /*scrolled_x=*/true, /*scrolled_y=*/true);
  std::optional<gfx::PointF> snap_point =
      scrollable_area->GetSnapPositionAndSetTarget(*strategy);
  if (snap_point.has_value()) {
    offset = scrollable_area->ScrollPositionToOffset(snap_point.value());
  }

  scrollable_area->SetScrollOffset(offset,
                                   mojom::blink::ScrollType::kProgrammatic,
                                   cc::ScrollSourceType::kAbsoluteScroll,
                                   mojom::blink::ScrollBehavior::kAuto);
}

void AdjustAreaOrContentInertness(const Element& element,
                                  bool is_overscroll_area,
                                  const ComputedStyle& parent_style,
                                  std::optional<bool>& html_inert,
                                  bool& can_escape_overscroll_inertness) {
  Element* parent = FlatTreeTraversal::ParentElement(element);
  if (!parent) {
    return;
  }

  auto* tracker = parent->GetOverscrollAreaTracker();

  if (is_overscroll_area) {
    // A closed overscroll area is always inert. An open overscroll area is
    // inert if another overscroll area above it in visual stacking order is
    // open.
    if (!element.MatchesOverscrollOpen()) {
      html_inert = true;
      can_escape_overscroll_inertness = !parent_style.IsInert();
    } else {
      CHECK(tracker);
      if (tracker->HasOpenAreaAbove(&element)) {
        html_inert = true;
      }
    }
    return;
  }

  // Regular content of an overscroll container is inerted when any of its
  // overscroll areas are open.
  if (tracker && tracker->HasAnyOpenArea()) {
    html_inert = true;
  }
}

void AdjustInvokerInertness(const Element& element,
                            const ComputedStyle& parent_style,
                            std::optional<bool>& html_inert) {
  if (html_inert.has_value() || !parent_style.CanEscapeOverscrollInertness()) {
    return;
  }

  auto* html_element = DynamicTo<HTMLElement>(&element);
  if (!html_element || !html_element->CanBeCommandInvoker()) {
    return;
  }

  // Command invokers are declaratively bound via HTML attributes (command and
  // commandfor) per the HTML Command Buttons specification.
  CommandEventType command = HTMLElement::GetCommandEventType(
      html_element->FastGetAttribute(html_names::kCommandAttr),
      html_element->GetExecutionContext());
  if (command != CommandEventType::kToggleOverscroll) {
    return;
  }

  Element* target = html_element->commandForElement();
  if (!target || target->GetTreeScope() != html_element->GetTreeScope()) {
    return;
  }

  Element* container = target->GetOverscrollContainer();
  if (!container) {
    return;
  }

  auto* tracker = container->GetOverscrollAreaTracker();
  CHECK(tracker);
  if (tracker->ShouldRemoveInertness(html_element, target)) {
    html_inert = false;
  }
}

bool IsValidOverscrollAreaInternal(
    Element& element,
    EInternalOverscrollPosition overscroll_position,
    EOverlay overlay,
    const ComputedStyle* parent_style) {
  if (overscroll_position != EInternalOverscrollPosition::kAuto) {
    return false;
  }
  if (!parent_style || parent_style->EffectiveOverscrollContainerType() ==
                           EOverscrollContainerType::kNone) {
    return false;
  }
  Element* parent = FlatTreeTraversal::ParentElement(element);
  if (!parent || parent->GetTreeScope() != element.GetTreeScope()) {
    return false;
  }
  bool is_in_top_layer =
      RuntimeEnabledFeatures::OverlayPropertyEnabled()
          ? (element.IsInTopLayer() && overlay == EOverlay::kAuto)
          : (element.IsInTopLayer() && element.IsRenderedInTopLayer());
  if (is_in_top_layer) {
    return false;
  }
  return element.GetDocument().IsOverscrollCommandTarget(element);
}

}  // namespace

OverscrollAreaTracker::OverscrollAreaTracker(Element* element)
    : container_(element) {}

void OverscrollAreaTracker::AddOverscroll(Element* element) {
  CHECK(!element->GetOverscrollContainer());
  DCHECK(element->isConnected());
  element->SetOverscrollContainer(container_);
  overscroll_members_.push_back(element);
  container_->SetNeedsReattachLayoutTree();
  needs_dom_sort_ = overscroll_members_.size() > 1;
}

const VectorOf<Element>& OverscrollAreaTracker::DOMSortedElements() {
  if (needs_dom_sort_) {
    std::sort(overscroll_members_.begin(), overscroll_members_.end(),
              [](const Member<Element>& a, const Member<Element>& b) {
                return a->compareDocumentPosition(b) &
                       Node::kDocumentPositionFollowing;
              });
    needs_dom_sort_ = false;
  }
  return overscroll_members_;
}

// static
bool OverscrollAreaTracker::IsValidOverscrollArea(
    Element& element,
    const ComputedStyleBuilder& style_builder,
    const ComputedStyle* parent_style) {
  return IsValidOverscrollAreaInternal(
      element, style_builder.InternalOverscrollPosition(),
      style_builder.Overlay(), parent_style);
}

// static
bool OverscrollAreaTracker::IsValidOverscrollArea(
    Element& element,
    const ComputedStyle* style,
    const ComputedStyle* parent_style) {
  return style && IsValidOverscrollAreaInternal(
                      element, style->InternalOverscrollPosition(),
                      style->Overlay(), parent_style);
}

// static
void OverscrollAreaTracker::AdjustInertness(
    const Element& element,
    bool is_overscroll_area,
    const ComputedStyle& parent_style,
    std::optional<bool>& html_inert,
    bool& can_escape_overscroll_inertness) {
  AdjustAreaOrContentInertness(element, is_overscroll_area, parent_style,
                               html_inert, can_escape_overscroll_inertness);
  AdjustInvokerInertness(element, parent_style, html_inert);
}

bool OverscrollAreaTracker::HasOpenAreaAbove(const Element* area) {
  DCHECK(area);
  DCHECK(overscroll_members_.Contains(area));
  for (Element* member : DOMSortedElements()) {
    if (member == area) {
      break;
    }
    if (member->MatchesOverscrollOpen()) {
      return true;
    }
  }
  return false;
}

bool OverscrollAreaTracker::HasAnyOpenArea() const {
  for (Element* member : overscroll_members_) {
    if (member->MatchesOverscrollOpen()) {
      return true;
    }
  }
  return false;
}

const Element* OverscrollAreaTracker::ContainingOverscrollArea(
    const Element* element) const {
  for (const Element* current = element; current;
       current = FlatTreeTraversal::ParentElement(*current)) {
    if (FlatTreeTraversal::ParentElement(*current) == container_) {
      return overscroll_members_.Contains(current) ? current : nullptr;
    }
  }
  return nullptr;
}

bool OverscrollAreaTracker::ShouldRemoveInertness(const Element* invoker,
                                                  const Element* target) {
  CHECK(invoker);
  CHECK(target);
  DCHECK(overscroll_members_.Contains(target));
  if (invoker->GetTreeScope() != target->GetTreeScope()) {
    return false;
  }
  // If the container itself is inert (e.g. via modal dialog or the inert
  // attribute), nothing inside it should have inertness removed.
  if (container_->GetComputedStyle() &&
      container_->GetComputedStyle()->IsInert()) {
    return false;
  }

  // Only a toggle invoker inside a closed overscroll area escapes inertness
  // (e.g. a handle or tab peaking out when the area is closed), provided it is
  // not covered by an open area above it in visual stacking order.
  if (ContainingOverscrollArea(invoker) == target &&
      !target->MatchesOverscrollOpen() && !HasOpenAreaAbove(target)) {
    return true;
  }

  return false;
}

void OverscrollAreaTracker::RemoveAllOverscroll() {
  for (auto& member : overscroll_members_) {
    member->ClearOverscrollContainer();
  }
  overscroll_members_.clear();
  needs_dom_sort_ = false;
}

void OverscrollAreaTracker::RemoveOverscroll(Element* element) {
  CHECK_EQ(element->GetOverscrollContainer(), container_);
  element->ClearOverscrollContainer();
  Erase(overscroll_members_, element);
  needs_dom_sort_ = needs_dom_sort_ && overscroll_members_.size() > 1;
}

void OverscrollAreaTracker::ToggleArea(Element* overscroll_area) {
  CHECK(RuntimeEnabledFeatures::OverscrollGesturesEnabled());
  auto* scrollable_area = GetScrollableAreaFor(overscroll_area);
  if (!scrollable_area) {
    return;
  }

  const cc::SnapContainerData* container_data =
      scrollable_area->GetSnapContainerData();
  CHECK(container_data && container_data->size() >= 2);

  const cc::TargetSnapAreaElementIds& previous_snap_targets =
      container_data->GetTargetSnapAreaElementIds();
  const auto& first_data = container_data->at(0);

  if (previous_snap_targets.x == first_data.element_id &&
      previous_snap_targets.y == first_data.element_id) {
    OpenArea(overscroll_area);
  } else {
    CloseArea(overscroll_area);
  }
}

void OverscrollAreaTracker::OpenArea(Element* overscroll_area) {
  CHECK(RuntimeEnabledFeatures::OverscrollGesturesEnabled());
  auto* scrollable_area = GetScrollableAreaFor(overscroll_area);
  if (!scrollable_area) {
    return;
  }

  const cc::SnapContainerData* container_data =
      scrollable_area->GetSnapContainerData();
  if (!container_data || container_data->size() < 2) {
    return;
  }

  auto* overscroll_area_parent =
      overscroll_area->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  auto* overscroll_area_object =
      DynamicTo<LayoutBox>(overscroll_area_parent->GetLayoutObject());
  if (!overscroll_area_object) {
    return;
  }

  const gfx::PointF scroll_origin(scrollable_area->ScrollOrigin());
  const gfx::RectF target_rect = container_data->at(1).rect;
  const PhysicalSize box_size =
      overscroll_area_object->PhysicalContentBoxRect().size;

  float min_x_offset = std::min(target_rect.x() - scroll_origin.x(), 0.f);
  float min_y_offset = std::min(target_rect.y() - scroll_origin.y(), 0.f);
  float max_x_offset = std::max(
      target_rect.right() - box_size.width.ToFloat() - scroll_origin.x(), 0.f);
  float max_y_offset = std::max(
      target_rect.bottom() - box_size.height.ToFloat() - scroll_origin.y(),
      0.f);

  ScrollOffset new_offset;
  if (std::max(-min_x_offset, max_x_offset) >
      std::max(-min_y_offset, max_y_offset)) {
    new_offset.set_x(-min_x_offset >= max_x_offset ? min_x_offset
                                                   : max_x_offset);
  } else {
    new_offset.set_y(-min_y_offset >= max_y_offset ? min_y_offset
                                                   : max_y_offset);
  }

  ScrollTo(scrollable_area, new_offset);
}

void OverscrollAreaTracker::CloseArea(Element* overscroll_area) {
  CHECK(RuntimeEnabledFeatures::OverscrollGesturesEnabled());
  auto* scrollable_area = GetScrollableAreaFor(overscroll_area);
  if (!scrollable_area) {
    return;
  }
  ScrollTo(scrollable_area, ScrollOffset());
}

void OverscrollAreaTracker::CloseAllAreas() {
  for (auto& member : overscroll_members_) {
    CloseArea(member);
  }
}

void OverscrollAreaTracker::Trace(Visitor* visitor) const {
  NodeRareDataField::Trace(visitor);

  visitor->Trace(container_);
  visitor->Trace(overscroll_members_);
}

}  // namespace blink
