// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_button_pseudo_element.h"

#include "cc/input/scroll_snap_data.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

namespace {

gfx::PointF CalculateSnappedScrollPosition(
    const ScrollableArea* scrollable_area,
    gfx::Vector2dF scaled_delta) {
  gfx::PointF current_position = scrollable_area->ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndAndDirection(
          current_position, scaled_delta,
          RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
  current_position += scaled_delta;
  if (std::optional<cc::SnapPositionData> snap_position =
          scrollable_area->GetSnapPosition(*strategy)) {
    if (snap_position->type != cc::SnapPositionData::Type::kNone) {
      current_position = snap_position->position;
    }
  }
  current_position.SetToMax(gfx::PointF());
  current_position.SetToMin(scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset()));
  return current_position;
}

}  // namespace

ScrollButtonPseudoElement::ScrollButtonPseudoElement(
    Element* originating_element,
    PseudoId pseudo_id)
    : PseudoElement(originating_element, pseudo_id),
      ScrollSnapshotClient(originating_element->GetDocument().GetFrame()) {
  SetTabIndexExplicitly();
}

void ScrollButtonPseudoElement::Trace(Visitor* v) const {
  PseudoElement::Trace(v);
}

void ScrollButtonPseudoElement::DefaultEventHandler(Event& event) {
  bool is_click =
      event.IsMouseEvent() && event.type() == event_type_names::kClick;
  bool is_key_down =
      event.IsKeyboardEvent() && event.type() == event_type_names::kKeydown;
  bool is_enter_or_space =
      is_key_down && (To<KeyboardEvent>(event).keyCode() == VKEY_RETURN ||
                      To<KeyboardEvent>(event).keyCode() == VKEY_SPACE);

  Element* scrolling_element = UltimateOriginatingElement();
  auto* scroller = DynamicTo<LayoutBox>(scrolling_element->GetLayoutObject());

  bool should_intercept = scroller && scroller->IsScrollContainer() &&
                          event.target() == this &&
                          (is_click || is_enter_or_space);
  if (should_intercept) {
    ScrollableArea* scrollable_area = scroller->GetScrollableArea();

    LogicalToPhysical<bool> mapping(
        scrolling_element->GetComputedStyle()->GetWritingDirection(),
        GetPseudoId() == kPseudoIdScrollButtonInlineStart,
        GetPseudoId() == kPseudoIdScrollButtonInlineEnd,
        GetPseudoId() == kPseudoIdScrollButtonBlockStart,
        GetPseudoId() == kPseudoIdScrollButtonBlockEnd);
    if (mapping.Top()) {
      scrolling_element->scrollBy(
          0, -scrollable_area->ScrollStep(ui::ScrollGranularity::kScrollByPage,
                                          kVerticalScrollbar));
    } else if (mapping.Bottom()) {
      scrolling_element->scrollBy(
          0, scrollable_area->ScrollStep(ui::ScrollGranularity::kScrollByPage,
                                         kVerticalScrollbar));
    } else if (mapping.Left()) {
      scrolling_element->scrollBy(
          -scrollable_area->ScrollStep(ui::ScrollGranularity::kScrollByPage,
                                       kHorizontalScrollbar),
          0);
    } else if (mapping.Right()) {
      scrolling_element->scrollBy(
          scrollable_area->ScrollStep(ui::ScrollGranularity::kScrollByPage,
                                      kHorizontalScrollbar),
          0);
    }
    GetDocument().SetFocusedElement(this,
                                    FocusParams(SelectionBehaviorOnFocus::kNone,
                                                mojom::blink::FocusType::kNone,
                                                /*capabilities=*/nullptr));
    event.SetDefaultHandled();
  }
  PseudoElement::DefaultEventHandler(event);
}

bool ScrollButtonPseudoElement::UpdateSnapshotInternal() {
  // Note: we can hit it here, since we don't unsubscribe from
  // scroll snapshot client (maybe we should).
  if (!isConnected()) {
    return true;
  }
  Element* originating_element = UltimateOriginatingElement();
  CHECK(originating_element);
  auto* scroller = DynamicTo<LayoutBox>(originating_element->GetLayoutObject());
  if (!scroller || !scroller->IsScrollContainer()) {
    return true;
  }
  ScrollableArea* scrollable_area = scroller->GetScrollableArea();
  LogicalToPhysical<bool> mapping(
      scroller->StyleRef().GetWritingDirection(),
      GetPseudoId() == kPseudoIdScrollButtonInlineStart,
      GetPseudoId() == kPseudoIdScrollButtonInlineEnd,
      GetPseudoId() == kPseudoIdScrollButtonBlockStart,
      GetPseudoId() == kPseudoIdScrollButtonBlockEnd);

  bool enabled = enabled_;
  if (mapping.Top()) {
    enabled_ = scrollable_area->ScrollPosition().y() >
               CalculateSnappedScrollPosition(
                   scrollable_area,
                   gfx::Vector2dF(0, -scrollable_area->ScrollStep(
                                         ui::ScrollGranularity::kScrollByPage,
                                         kVerticalScrollbar)))
                   .y();
  } else if (mapping.Bottom()) {
    enabled_ = scrollable_area->ScrollPosition().y() <
               CalculateSnappedScrollPosition(
                   scrollable_area,
                   gfx::Vector2dF(0, scrollable_area->ScrollStep(
                                         ui::ScrollGranularity::kScrollByPage,
                                         kVerticalScrollbar)))
                   .y();
  } else if (mapping.Left()) {
    enabled_ = scrollable_area->ScrollPosition().x() >
               CalculateSnappedScrollPosition(
                   scrollable_area,
                   gfx::Vector2dF(-scrollable_area->ScrollStep(
                                      ui::ScrollGranularity::kScrollByPage,
                                      kHorizontalScrollbar),
                                  0))
                   .x();
  } else if (mapping.Right()) {
    enabled_ = scrollable_area->ScrollPosition().x() <
               CalculateSnappedScrollPosition(
                   scrollable_area,
                   gfx::Vector2dF(scrollable_area->ScrollStep(
                                      ui::ScrollGranularity::kScrollByPage,
                                      kHorizontalScrollbar),
                                  0))
                   .x();
  }
  if (enabled != enabled_) {
    SetNeedsStyleRecalc(
        StyleChangeType::kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kControl));
    return false;
  }
  return true;
}

void ScrollButtonPseudoElement::UpdateSnapshot() {
  UpdateSnapshotInternal();
}

bool ScrollButtonPseudoElement::ValidateSnapshot() {
  return UpdateSnapshotInternal();
}

bool ScrollButtonPseudoElement::ShouldScheduleNextService() {
  return false;
}

}  // namespace blink
