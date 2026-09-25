// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_button_pseudo_element.h"

#include <memory>
#include <optional>

#include "cc/input/scroll_snap_data.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

PseudoId ScrollButtonPseudoElement::PseudoIdFromScrollButtonArgument(
    const AtomicString& argument,
    const ComputedStyle& originating_element_style) {
  DEFINE_STATIC_LOCAL(AtomicString, wildcard, ("*"));
  DEFINE_STATIC_LOCAL(AtomicString, block_start, ("block-start"));
  DEFINE_STATIC_LOCAL(AtomicString, inline_start, ("inline-start"));
  DEFINE_STATIC_LOCAL(AtomicString, inline_end, ("inline-end"));
  DEFINE_STATIC_LOCAL(AtomicString, block_end, ("block-end"));

  if (argument == wildcard) {
    return kPseudoIdScrollButton;
  }
  if (argument == block_start) {
    return kPseudoIdScrollButtonBlockStart;
  }
  if (argument == inline_start) {
    return kPseudoIdScrollButtonInlineStart;
  }
  if (argument == inline_end) {
    return kPseudoIdScrollButtonInlineEnd;
  }
  if (argument == block_end) {
    return kPseudoIdScrollButtonBlockEnd;
  }

  DEFINE_STATIC_LOCAL(AtomicString, up, ("up"));
  DEFINE_STATIC_LOCAL(AtomicString, right, ("right"));
  DEFINE_STATIC_LOCAL(AtomicString, down, ("down"));
  DEFINE_STATIC_LOCAL(AtomicString, left, ("left"));

  if (argument != up && argument != right && argument != down &&
      argument != left) {
    return kPseudoIdScrollButton;
  }

  PhysicalToLogical<bool> mapping(
      originating_element_style.GetWritingDirection(), argument == up,
      argument == right, argument == down, argument == left);
  if (mapping.BlockStart()) {
    return kPseudoIdScrollButtonBlockStart;
  }
  if (mapping.InlineStart()) {
    return kPseudoIdScrollButtonInlineStart;
  }
  if (mapping.InlineEnd()) {
    return kPseudoIdScrollButtonInlineEnd;
  }
  CHECK(mapping.BlockEnd());
  return kPseudoIdScrollButtonBlockEnd;
}

namespace {

PaintLayerScrollableArea* ScrollableAreaForScroller(const LayoutBox& scroller) {
  return scroller.IsDocumentElement()
             ? scroller.GetFrameView()->LayoutViewport()
             : scroller.GetScrollableArea();
}

std::optional<ScrollDirectionPhysical> ScrollDirectionForButton(
    PseudoId pseudo_id,
    WritingDirectionMode writing_direction) {
  const PhysicalToLogical<ScrollDirectionPhysical> directions(
      writing_direction, kScrollUp, kScrollRight, kScrollDown, kScrollLeft);
  switch (pseudo_id) {
    case kPseudoIdScrollButtonInlineStart:
      return directions.InlineStart();
    case kPseudoIdScrollButtonInlineEnd:
      return directions.InlineEnd();
    case kPseudoIdScrollButtonBlockStart:
      return directions.BlockStart();
    case kPseudoIdScrollButtonBlockEnd:
      return directions.BlockEnd();
    default:
      return std::nullopt;
  }
}

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

bool ScrollButtonPseudoElement::HandleButtonActivation() {
  if (!isConnected() || !parentElement()) {
    return false;
  }
  Element& scrolling_element = UltimateOriginatingElement();
  LayoutBox* scroller = scrolling_element.GetLayoutBox();
  PaintLayerScrollableArea* scrollable_area =
      ScrollableAreaForScroller(*scroller);
  // Future proof in case of possibility to activate scroll button
  // without an appropriate scroller via a click event from JS.
  if (!scrollable_area) {
    return false;
  }

  if (const auto direction = ScrollDirectionForButton(
          GetPseudoId(),
          scrolling_element.GetComputedStyle()->GetWritingDirection())) {
    scrollable_area->ScrollByPageWithSnap(*direction);
  }
  GetDocument().SetFocusedElement(this,
                                  FocusParams(SelectionBehaviorOnFocus::kNone,
                                              mojom::blink::FocusType::kNone,
                                              /*capabilities=*/nullptr));
  return true;
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
  if (should_intercept && HandleButtonActivation()) {
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

bool ScrollButtonPseudoElement::CalculateEnabledState() const {
  // Detached buttons can remain registered for snapshot updates.
  // Preserve their last enabled state.
  if (!isConnected() || !parentElement()) {
    return enabled_;
  }
  const LayoutBox* scroller = UltimateOriginatingElement().GetLayoutBox();
  if (!scroller ||
      (!scroller->IsScrollContainer() && !scroller->IsDocumentElement())) {
    return false;
  }
  const PaintLayerScrollableArea* scrollable_area =
      ScrollableAreaForScroller(*scroller);
  CHECK(scrollable_area);
  // Compare both positions after rounding to the nearest pixel.
  const ScrollOffset current_position = gfx::ToRoundedVector2d(
      scrollable_area->ScrollPosition().OffsetFromOrigin());
  const auto direction = ScrollDirectionForButton(
      GetPseudoId(), scroller->StyleRef().GetWritingDirection());
  if (!direction) {
    return enabled_;
  }

  const ScrollOffset target_position =
      CalculateSnappedScrollPosition(scrollable_area, *direction);
  switch (*direction) {
    case kScrollUp:
      return target_position.y() < current_position.y();
    case kScrollDown:
      return target_position.y() > current_position.y();
    case kScrollLeft:
      return target_position.x() < current_position.x();
    case kScrollRight:
      return target_position.x() > current_position.x();
  }
  return enabled_;
}

bool ScrollButtonPseudoElement::UpdateSnapshot() {
  const bool new_enabled = CalculateEnabledState();
  if (new_enabled == enabled_) {
    return false;
  }
  enabled_ = new_enabled;
  SetNeedsStyleRecalc(
      StyleChangeType::kLocalStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kControl));
  return true;
}

bool ScrollButtonPseudoElement::ShouldScheduleNextService() {
  return false;
}

}  // namespace blink
