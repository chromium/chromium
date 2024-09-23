/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_custom_scrollbar_part.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/custom_scrollbar_theme.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

CustomScrollbar::CustomScrollbar(ScrollableArea* scrollable_area,
                                 ScrollbarOrientation orientation,
                                 const LayoutObject* style_source,
                                 bool suppress_use_counters)
    : Scrollbar(scrollable_area,
                orientation,
                style_source,
                CustomScrollbarTheme::GetCustomScrollbarTheme()),
      suppress_use_counters_(suppress_use_counters) {
  DCHECK(style_source);
}

CustomScrollbar::~CustomScrollbar() {
  DCHECK(!scrollable_area_);
  DCHECK(parts_.empty());
}

int CustomScrollbar::HypotheticalScrollbarThickness(
    const ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation,
    const LayoutObject* style_source) {
  // Create a temporary scrollbar so that we can match style rules like
  // ::-webkit-scrollbar:horizontal according to the scrollbar's orientation.
  auto* scrollbar = MakeGarbageCollected<CustomScrollbar>(
      const_cast<ScrollableArea*>(scrollable_area), orientation, style_source,
      /* suppress_use_counters */ true);
  scrollbar->UpdateScrollbarPart(kScrollbarBGPart);
  auto* part = scrollbar->GetPart(kScrollbarBGPart);
  int thickness = part ? part->ComputeThickness() : 0;
  scrollbar->DisconnectFromScrollableArea();
  return thickness;
}

void CustomScrollbar::Trace(Visitor* visitor) const {
  visitor->Trace(parts_);
  Scrollbar::Trace(visitor);
}

void CustomScrollbar::DisconnectFromScrollableArea() {
  DestroyScrollbarParts();
  Scrollbar::DisconnectFromScrollableArea();
}

void CustomScrollbar::SetEnabled(bool enabled) {
  if (Enabled() == enabled)
    return;
  Scrollbar::SetEnabled(enabled);
  UpdateScrollbarParts();
}

void CustomScrollbar::StyleChanged() {
  UpdateScrollbarParts();
}

void CustomScrollbar::SetHoveredPart(ScrollbarPart part) {
  // This can be called from EventHandler after the scrollbar has been
  // disconnected from the scrollable area.
  if (!scrollable_area_)
    return;

  if (part == hovered_part_)
    return;

  ScrollbarPart old_part = hovered_part_;
  hovered_part_ = part;

  UpdateScrollbarPart(old_part);
  UpdateScrollbarPart(hovered_part_);

  UpdateScrollbarPart(kScrollbarBGPart);
  UpdateScrollbarPart(kTrackBGPart);

  PositionScrollbarParts();
}

void CustomScrollbar::SetPressedPart(ScrollbarPart part,
                                     WebInputEvent::Type type) {
  // This can be called from EventHandler after the scrollbar has been
  // disconnected from the scrollable area.
  if (!scrollable_area_)
    return;

  ScrollbarPart old_part = pressed_part_;
  Scrollbar::SetPressedPart(part, type);

  UpdateScrollbarPart(old_part);
  UpdateScrollbarPart(part);

  UpdateScrollbarPart(kScrollbarBGPart);
  UpdateScrollbarPart(kTrackBGPart);

  PositionScrollbarParts();
}

const ComputedStyle* CustomScrollbar::GetScrollbarPseudoElementStyle(
    ScrollbarPart part_type,
    PseudoId pseudo_id) {
  const LayoutObject* layout_object = StyleSource();
  DCHECK(layout_object);
  Document& document = layout_object->GetDocument();
  if (!document.InStyleRecalc()) {
    // We are currently querying style for custom scrollbars on a style-dirty
    // tree outside style recalc. Update active style to make sure we don't
    // crash on null RuleSets.
    // TODO(crbug.com/1114644): We should not compute style for a dirty tree
    // outside the lifecycle update. Instead we should mark the originating
    // element for style recalc and let the next lifecycle update compute the
    // scrollbar styles.
    document.GetStyleEngine().UpdateActiveStyle();
  }
  const ComputedStyle& source_style = layout_object->StyleRef();
  const ComputedStyle* part_style =
      layout_object->GetUncachedPseudoElementStyle(
          StyleRequest(pseudo_id, this, part_type, &source_style));
  if (!part_style)
    return nullptr;
  if (part_style->DependsOnFontMetrics()) {
    if (Element* element = DynamicTo<Element>(layout_object->GetNode())) {
      element->SetScrollbarPseudoElementStylesDependOnFontMetrics(true);
    }
  }
  return part_style;
}

void CustomScrollbar::DestroyScrollbarParts() {
  for (auto& part : parts_)
    part.value->Destroy();
  parts_.clear();
}

void CustomScrollbar::UpdateScrollbarParts() {
  for (auto part :
       {kScrollbarBGPart, kBackButtonStartPart, kForwardButtonStartPart,
        kBackTrackPart, kThumbPart, kForwardTrackPart, kBackButtonEndPart,
        kForwardButtonEndPart, kTrackBGPart})
    UpdateScrollbarPart(part);

  // See if the scrollbar's thickness changed.  If so, we need to mark our
  // owning object as needing a layout.
  bool is_horizontal = Orientation() == kHorizontalScrollbar;
  int old_thickness = is_horizontal ? Height() : Width();
  int new_thickness = 0;
  auto it = parts_.find(kScrollbarBGPart);
  if (it != parts_.end())
    new_thickness = it->value->ComputeThickness();

  if (new_thickness != old_thickness) {
    SetFrameRect(gfx::Rect(
        Location(), gfx::Size(is_horizontal ? Width() : new_thickness,
                              is_horizontal ? new_thickness : Height())));
    if (LayoutBox* box = GetLayoutBox()) {
      box->SetChildNeedsLayout();
      // LayoutNG may attempt to reuse line-box fragments. It will do this even
      // if the |LayoutObject::ChildNeedsLayout| is true (set above).
      // The box itself needs to be marked as needs layout here, as conceptually
      // this is similar to border or padding changing, (which marks the box as
      // self needs layout).
      box->SetNeedsLayout(layout_invalidation_reason::kScrollbarChanged);
      scrollable_area_->SetScrollCornerNeedsPaintInvalidation();
    }
    return;
  }

  // If we didn't return above, it means that there is no change or the change
  // doesn't affect layout of the box. Update position to reflect the change if
  // any.
  if (LayoutBox* box = GetLayoutBox()) {
    // It's not ready to position scrollbar parts if the containing box has not
    // been inserted into the layout tree.
    if (box->IsLayoutView() || box->Parent())
      PositionScrollbarParts();
  }
}

static PseudoId PseudoForScrollbarPart(ScrollbarPart part) {
  switch (part) {
    case kBackButtonStartPart:
    case kForwardButtonStartPart:
    case kBackButtonEndPart:
    case kForwardButtonEndPart:
      return kPseudoIdScrollbarButton;
    case kBackTrackPart:
    case kForwardTrackPart:
      return kPseudoIdScrollbarTrackPiece;
    case kThumbPart:
      return kPseudoIdScrollbarThumb;
    case kTrackBGPart:
      return kPseudoIdScrollbarTrack;
    case kScrollbarBGPart:
      return kPseudoIdScrollbar;
    case kNoPart:
    case kAllParts:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return kPseudoIdScrollbar;
}

void CustomScrollbar::UpdateScrollbarPart(ScrollbarPart part_type) {
  DCHECK(scrollable_area_);
  if (part_type == kNoPart)
    return;

  const ComputedStyle* part_style = GetScrollbarPseudoElementStyle(
      part_type, PseudoForScrollbarPart(part_type));
  bool need_layout_object =
      part_style && part_style->Display() != EDisplay::kNone;

  if (need_layout_object &&
      // display:block overrides OS settings.
      part_style->Display() != EDisplay::kBlock) {
    // If not display:block, visibility of buttons depends on OS settings.
    switch (part_type) {
      case kBackButtonStartPart:
      case kForwardButtonEndPart:
        // Create buttons only if the OS theme has scrollbar buttons.
        need_layout_object = GetTheme().NativeThemeHasButtons();
        break;
      case kBackButtonEndPart:
      case kForwardButtonStartPart:
        // These buttons are not supported by any OS.
        need_layout_object = false;
        break;
      default:
        break;
    }
  }

  auto it = parts_.find(part_type);
  LayoutCustomScrollbarPart* part_layout_object =
      it != parts_.end() ? it->value : nullptr;
  if (!part_layout_object && need_layout_object && scrollable_area_) {
    part_layout_object = LayoutCustomScrollbarPart::CreateAnonymous(
        &StyleSource()->GetDocument(), scrollable_area_, this, part_type,
        suppress_use_counters_);
    parts_.Set(part_type, part_layout_object);
    SetNeedsPaintInvalidation(part_type);
  } else if (part_layout_object && !need_layout_object) {
    parts_.erase(part_type);
    part_layout_object->Destroy();
    part_layout_object = nullptr;
    SetNeedsPaintInvalidation(part_type);
  }

  if (part_layout_object)
    part_layout_object->SetStyle(part_style);
}

gfx::Rect CustomScrollbar::ButtonRect(ScrollbarPart part_type) const {
  auto it = parts_.find(part_type);
  if (it == parts_.end())
    return gfx::Rect();

  bool is_horizontal = Orientation() == kHorizontalScrollbar;
  int button_length = it->value->ComputeLength();
  gfx::Rect button_rect(Location(), is_horizontal
                                        ? gfx::Size(button_length, Height())
                                        : gfx::Size(Width(), button_length));

  switch (part_type) {
    case kBackButtonStartPart:
      break;
    case kForwardButtonEndPart:
      button_rect.Offset(is_horizontal ? Width() - button_length : 0,
                         is_horizontal ? 0 : Height() - button_length);
      break;
    case kForwardButtonStartPart: {
      gfx::Rect previous_button = ButtonRect(kBackButtonStartPart);
      button_rect.Offset(is_horizontal ? previous_button.width() : 0,
                         is_horizontal ? 0 : previous_button.height());
      break;
    }
    case kBackButtonEndPart: {
      gfx::Rect next_button = ButtonRect(kForwardButtonEndPart);
      button_rect.Offset(
          is_horizontal ? Width() - next_button.width() - button_length : 0,
          is_horizontal ? 0 : Height() - next_button.height() - button_length);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return button_rect;
}

gfx::Rect CustomScrollbar::TrackRect(int start_length, int end_length) const {
  const LayoutCustomScrollbarPart* part = GetPart(kTrackBGPart);

  if (Orientation() == kHorizontalScrollbar) {
    int margin_left = part ? part->MarginLeft().ToInt() : 0;
    int margin_right = part ? part->MarginRight().ToInt() : 0;
    start_length += margin_left;
    end_length += margin_right;
    int total_length = start_length + end_length;
    return gfx::Rect(X() + start_length, Y(), Width() - total_length, Height());
  }

  int margin_top = part ? part->MarginTop().ToInt() : 0;
  int margin_bottom = part ? part->MarginBottom().ToInt() : 0;
  start_length += margin_top;
  end_length += margin_bottom;
  int total_length = start_length + end_length;

  return gfx::Rect(X(), Y() + start_length, Width(), Height() - total_length);
}

gfx::Rect CustomScrollbar::TrackPieceRectWithMargins(
    ScrollbarPart part_type,
    const gfx::Rect& old_rect) const {
  const LayoutCustomScrollbarPart* part_layout_object = GetPart(part_type);
  if (!part_layout_object)
    return old_rect;

  gfx::Rect rect = old_rect;
  if (Orientation() == kHorizontalScrollbar) {
    rect.set_x((rect.x() + part_layout_object->MarginLeft()).ToInt());
    rect.set_width((rect.width() - part_layout_object->MarginWidth()).ToInt());
  } else {
    rect.set_y((rect.y() + part_layout_object->MarginTop()).ToInt());
    rect.set_height(
        (rect.height() - part_layout_object->MarginHeight()).ToInt());
  }
  return rect;
}

int CustomScrollbar::MinimumThumbLength() const {
  if (const auto* part_layout_object = GetPart(kThumbPart))
    return part_layout_object->ComputeLength();
  return 0;
}

void CustomScrollbar::OffsetDidChange(mojom::blink::ScrollType scroll_type) {
  Scrollbar::OffsetDidChange(scroll_type);
  PositionScrollbarParts();
}

void CustomScrollbar::PositionScrollbarParts() {
  DCHECK_NE(
      scrollable_area_->GetLayoutBox()->GetDocument().Lifecycle().GetState(),
      DocumentLifecycle::kInPaint);

  // Update frame rect of parts.
  gfx::Rect track_rect = GetTheme().TrackRect(*this);
  gfx::Rect start_track_rect;
  gfx::Rect thumb_rect;
  gfx::Rect end_track_rect;
  GetTheme().SplitTrack(*this, track_rect, start_track_rect, thumb_rect,
                        end_track_rect);
  for (auto& part : parts_) {
    gfx::Rect part_rect;
    switch (part.key) {
      case kBackButtonStartPart:
      case kForwardButtonStartPart:
      case kBackButtonEndPart:
      case kForwardButtonEndPart:
        part_rect = ButtonRect(part.key);
        break;
      case kBackTrackPart:
        part_rect = start_track_rect;
        break;
      case kForwardTrackPart:
        part_rect = end_track_rect;
        break;
      case kThumbPart:
        part_rect = thumb_rect;
        break;
      case kTrackBGPart:
        part_rect = track_rect;
        break;
      case kScrollbarBGPart:
        part_rect = FrameRect();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    part.value->ClearNeedsLayoutWithoutPaintInvalidation();
    // The part's paint offset is relative to the box.
    // TODO(crbug.com/1020913): This should be part of PaintPropertyTreeBuilder
    // when we support subpixel layout of overflow controls.
    part.value->GetMutableForPainting().FirstFragment().SetPaintOffset(
        PhysicalOffset(part_rect.origin()));
    part.value->SetOverriddenSize(PhysicalSize(part_rect.size()));
  }
}

const ComputedStyle* CustomScrollbar::GetScrollbarPartStyleForCursor(
    ScrollbarPart part_type) const {
  const LayoutCustomScrollbarPart* part_layout_object = GetPart(part_type);
  if (part_layout_object) {
    return part_layout_object->Style();
  }
  switch (part_type) {
    case kBackButtonStartPart:
    case kForwardButtonStartPart:
    case kBackButtonEndPart:
    case kForwardButtonEndPart:
    case kTrackBGPart:
    case kThumbPart:
      return GetScrollbarPartStyleForCursor(kScrollbarBGPart);
    case kBackTrackPart:
    case kForwardTrackPart:
      return GetScrollbarPartStyleForCursor(kTrackBGPart);
    default:
      break;
  }
  return nullptr;
}

void CustomScrollbar::InvalidateDisplayItemClientsOfScrollbarParts() {
  for (auto& part : parts_) {
    DCHECK(!part.value->PaintingLayer());
    ObjectPaintInvalidator(*part.value)
        .InvalidateDisplayItemClient(*part.value,
                                     PaintInvalidationReason::kScrollControl);
  }
}

void CustomScrollbar::ClearPaintFlags() {
  for (auto& part : parts_)
    part.value->ClearPaintFlags();
}

void CustomScrollbar::Paint(GraphicsContext& context,
                            const PhysicalOffset& paint_offset) const {
  auto& theme = GetTheme();
  // TODO(crbug.com/40105990): We should not round paint_offset but should
  // consider subpixel accumulation when painting scrollbars.
  gfx::Vector2d offset = ToRoundedVector2d(paint_offset);
  theme.PaintTrackAndButtons(context, *this, FrameRect() + offset);
  if (theme.HasThumb(*this)) {
    theme.PaintThumb(context, *this, theme.ThumbRect(*this) + offset);
  }
}

}  // namespace blink
