// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_button_pseudo_element.h"

#include "cc/input/scroll_snap_data.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/public/mojom/scroll/scroll_enums.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

namespace {

ScrollOffset CalculateSnappedScrollPosition(
    const ScrollableArea* scrollable_area,
    ScrollDirectionPhysical direction) {
  gfx::PointF current_position = scrollable_area->ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      scrollable_area->PageScrollSnapStrategy(direction);
  gfx::Vector2dF displacement = ToScrollDelta(direction, 1);
  displacement.Scale(
      scrollable_area->ScrollStep(ui::ScrollGranularity::kScrollByPage,
                                  kHorizontalScrollbar),
      scrollable_area->ScrollStep(ui::ScrollGranularity::kScrollByPage,
                                  kVerticalScrollbar));

  current_position += displacement;
  if (std::optional<cc::SnapPositionData> snap_position =
          scrollable_area->GetSnapPosition(*strategy)) {
    if (snap_position->type != cc::SnapPositionData::Type::kNone) {
      current_position = snap_position->position;
    }
  }
  current_position.SetToMax(gfx::PointF());
  current_position.SetToMin(scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset()));
  return gfx::ToRoundedVector2d(current_position.OffsetFromOrigin());
}

}  // namespace

ScrollButtonPseudoElement::ScrollButtonPseudoElement(
    Element* originating_element,
    PseudoId pseudo_id)
    : PseudoElement(originating_element, pseudo_id),
      PostLayoutSnapshotClient(originating_element->GetDocument().GetFrame()) {
  SetTabIndexExplicitly();
  UseCounter::Count(GetDocument(), WebFeature::kScrollButtonPseudoElement);
}

void ScrollButtonPseudoElement::Trace(Visitor* v) const {
  PseudoElement::Trace(v);
}

void ScrollButtonPseudoElement::HandleButtonActivation() {
  Element& scrolling_element = UltimateOriginatingElement();
  LayoutBox* scroller = scrolling_element.GetLayoutBox();
  PaintLayerScrollableArea* scrollable_area =
      scroller->IsDocumentElement() ? scroller->GetFrameView()->LayoutViewport()
                                    : scroller->GetScrollableArea();
  // Future proof in case of possibility to activate scroll button
  // without an appropriate scroller via a click event from JS.
  if (!scrollable_area) {
    return;
  }

  LogicalToPhysical<bool> mapping(
      scrolling_element.GetComputedStyle()->GetWritingDirection(),
      GetPseudoId() == kPseudoIdScrollButtonInlineStart,
      GetPseudoId() == kPseudoIdScrollButtonInlineEnd,
      GetPseudoId() == kPseudoIdScrollButtonBlockStart,
      GetPseudoId() == kPseudoIdScrollButtonBlockEnd);
  std::optional<ScrollDirectionPhysical> direction;
  if (mapping.Top()) {
    direction = ScrollDirectionPhysical::kScrollUp;
  } else if (mapping.Bottom()) {
    direction = ScrollDirectionPhysical::kScrollDown;
  } else if (mapping.Left()) {
    direction = ScrollDirectionPhysical::kScrollLeft;
  } else if (mapping.Right()) {
    direction = ScrollDirectionPhysical::kScrollRight;
  }
  if (direction) {
    scrollable_area->ScrollByPageWithSnap(*direction);
  }
  GetDocument().SetFocusedElement(this,
                                  FocusParams(SelectionBehaviorOnFocus::kNone,
                                              mojom::blink::FocusType::kNone,
                                              /*capabilities=*/nullptr));
}

void ScrollButtonPseudoElement::DefaultEventHandler(Event& event) {
  bool is_click =
      event.IsMouseEvent() && event.type() == event_type_names::kClick;
  bool is_key_down =
      event.IsKeyboardEvent() && event.type() == event_type_names::kKeydown;
  bool is_enter_or_space =
      is_key_down && (To<KeyboardEvent>(event).keyCode() == VKEY_RETURN ||
                      To<KeyboardEvent>(event).keyCode() == VKEY_SPACE);
  bool should_intercept =
      event.RawTarget() == this && (is_click || is_enter_or_space);
  if (should_intercept) {
    HandleButtonActivation();
    event.SetDefaultHandled();
  }
  PseudoElement::DefaultEventHandler(event);
}

FocusableState ScrollButtonPseudoElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  if (IsDisabledFormControl()) {
    return FocusableState::kNotFocusable;
  }
  return PseudoElement::SupportsFocus(update_behavior);
}

bool ScrollButtonPseudoElement::UpdateSnapshot() {
  // Note: we can hit it here, since we don't unsubscribe from
  // scroll snapshot client (maybe we should).
  if (!isConnected()) {
    return false;
  }
  LayoutBox* scroller =
      DynamicTo<LayoutBox>(UltimateOriginatingElement().GetLayoutObject());
  if (!scroller ||
      (!scroller->IsScrollContainer() && !scroller->IsDocumentElement())) {
    // Make sure the scroll button is disabled if the originating element
    // is not an appropriate scroller.
    if (enabled_) {
      enabled_ = false;
      SetNeedsStyleRecalc(
          StyleChangeType::kLocalStyleChange,
          StyleChangeReasonForTracing::Create(style_change_reason::kControl));
      return true;
    }
    return false;
  }
  ScrollableArea* scrollable_area =
      scroller->IsDocumentElement() ? scroller->GetFrameView()->LayoutViewport()
                                    : scroller->GetScrollableArea();
  CHECK(scrollable_area);
  // Scrolls are rounded to the nearest offset pixel in
  // ScrollableArea::SetScrollOffset. We apply the same offsets in the
  // calculations here to ensure that the snap limit agrees between them.
  ScrollOffset current_position = gfx::ToRoundedVector2d(
      scrollable_area->ScrollPosition().OffsetFromOrigin());
  LogicalToPhysical<bool> mapping(
      scroller->StyleRef().GetWritingDirection(),
      GetPseudoId() == kPseudoIdScrollButtonInlineStart,
      GetPseudoId() == kPseudoIdScrollButtonInlineEnd,
      GetPseudoId() == kPseudoIdScrollButtonBlockStart,
      GetPseudoId() == kPseudoIdScrollButtonBlockEnd);

  bool enabled = enabled_;
  if (mapping.Top()) {
    enabled_ = current_position.y() >
               CalculateSnappedScrollPosition(
                   scrollable_area, ScrollDirectionPhysical::kScrollUp)
                   .y();
  } else if (mapping.Bottom()) {
    enabled_ = current_position.y() <
               CalculateSnappedScrollPosition(
                   scrollable_area, ScrollDirectionPhysical::kScrollDown)
                   .y();
  } else if (mapping.Left()) {
    enabled_ = current_position.x() >
               CalculateSnappedScrollPosition(
                   scrollable_area, ScrollDirectionPhysical::kScrollLeft)
                   .x();
  } else if (mapping.Right()) {
    enabled_ = current_position.x() <
               CalculateSnappedScrollPosition(
                   scrollable_area, ScrollDirectionPhysical::kScrollRight)
                   .x();
  }
  if (enabled != enabled_) {
    SetNeedsStyleRecalc(
        StyleChangeType::kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kControl));
    return true;
  }
  return false;
}

bool ScrollButtonPseudoElement::ShouldScheduleNextService() {
  return false;
}

}  // namespace blink
