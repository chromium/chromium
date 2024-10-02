/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_replaced.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view_transition_content.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/replaced_painter.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

const int LayoutReplaced::kDefaultWidth = 300;
const int LayoutReplaced::kDefaultHeight = 150;

LayoutReplaced::LayoutReplaced(Element* element)
    : LayoutBox(element),
      intrinsic_size_(LayoutUnit(kDefaultWidth), LayoutUnit(kDefaultHeight)) {
  // TODO(jchaffraix): We should not set this boolean for block-level
  // replaced elements (crbug.com/567964).
  SetIsAtomicInlineLevel(true);
}

LayoutReplaced::LayoutReplaced(Element* element,
                               const PhysicalSize& intrinsic_size)
    : LayoutBox(element), intrinsic_size_(intrinsic_size) {
  // TODO(jchaffraix): We should not set this boolean for block-level
  // replaced elements (crbug.com/567964).
  SetIsAtomicInlineLevel(true);
}

LayoutReplaced::~LayoutReplaced() = default;

void LayoutReplaced::WillBeDestroyed() {
  NOT_DESTROYED();
  if (!DocumentBeingDestroyed() && Parent())
    Parent()->DirtyLinesFromChangedChild(this);

  LayoutBox::WillBeDestroyed();
}

void LayoutReplaced::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBox::StyleDidChange(diff, old_style);

  // Replaced elements can have border-radius clips without clipping overflow;
  // the overflow clipping case is already covered in LayoutBox::StyleDidChange
  if (old_style && diff.BorderRadiusChanged()) {
    SetNeedsPaintPropertyUpdate();
  }

  bool had_style = !!old_style;
  float old_zoom = had_style ? old_style->EffectiveZoom()
                             : ComputedStyleInitialValues::InitialZoom();
  if (Style() && StyleRef().EffectiveZoom() != old_zoom)
    IntrinsicSizeChanged();

  if ((IsLayoutImage() || IsVideo() || IsCanvas()) && !ClipsToContentBox() &&
      !StyleRef().ObjectPropertiesPreventReplacedOverflow()) {
    static constexpr const char kErrorMessage[] =
        "Specifying 'overflow: visible' on img, video and canvas tags may "
        "cause them to produce visual content outside of the element bounds. "
        "See "
        "https://github.com/WICG/view-transitions/blob/main/"
        "debugging_overflow_on_images.md for details.";
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning, kErrorMessage);
    constexpr bool kDiscardDuplicates = true;
    GetDocument().AddConsoleMessage(console_message, kDiscardDuplicates);
  }
}

void LayoutReplaced::IntrinsicSizeChanged() {
  NOT_DESTROYED();
  LayoutUnit scaled_width =
      LayoutUnit(static_cast<int>(kDefaultWidth * StyleRef().EffectiveZoom()));
  LayoutUnit scaled_height =
      LayoutUnit(static_cast<int>(kDefaultHeight * StyleRef().EffectiveZoom()));
  intrinsic_size_ = PhysicalSize(scaled_width, scaled_height);
  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kSizeChanged);
}

void LayoutReplaced::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  ReplacedPainter(*this).Paint(paint_info);
}

static inline bool LayoutObjectHasIntrinsicAspectRatio(
    const LayoutObject* layout_object) {
  DCHECK(layout_object);
  return layout_object->IsImage() || layout_object->IsCanvas() ||
         IsA<LayoutVideo>(layout_object) ||
         IsA<LayoutViewTransitionContent>(layout_object);
}

void LayoutReplaced::AddVisualEffectOverflow() {
  NOT_DESTROYED();
  if (!StyleRef().HasVisualOverflowingEffect()) {
    return;
  }

  // Add in the final overflow with shadows, outsets and outline combined.
  PhysicalRect visual_effect_overflow = PhysicalBorderBoxRect();
  PhysicalBoxStrut outsets = ComputeVisualEffectOverflowOutsets();
  visual_effect_overflow.Expand(outsets);
  AddSelfVisualOverflow(visual_effect_overflow);
  UpdateHasSubpixelVisualEffectOutsets(outsets);
}

void LayoutReplaced::RecalcVisualOverflow() {
  NOT_DESTROYED();
  ClearVisualOverflow();
  LayoutObject::RecalcVisualOverflow();
  AddVisualEffectOverflow();

  // Replaced elements clip the content to the element's content-box by default.
  // But if the CSS overflow property is respected, the content may paint
  // outside the element's bounds as ink overflow (with overflow:visible for
  // example). So we add |ReplacedContentRect()|, which provides the element's
  // painting rectangle relative to it's bounding box in its visual overflow if
  // the overflow property is respected.
  // Note that |overflow_| is meant to track the maximum potential ink overflow.
  // The actual painted overflow (based on the values for overflow,
  // overflow-clip-margin and paint containment) is computed in
  // LayoutBox::VisualOverflowRect.
  if (RespectsCSSOverflow())
    AddContentsVisualOverflow(ReplacedContentRect());
}

std::optional<PhysicalRect> LayoutReplaced::ComputeObjectViewBoxRect(
    const PhysicalSize* overridden_intrinsic_size) const {
  const BasicShape* object_view_box = StyleRef().ObjectViewBox();
  if (!object_view_box) [[likely]] {
    return std::nullopt;
  }

  const auto& intrinsic_size =
      overridden_intrinsic_size ? *overridden_intrinsic_size : intrinsic_size_;
  if (intrinsic_size.IsEmpty())
    return std::nullopt;

  if (!CanApplyObjectViewBox())
    return std::nullopt;

  DCHECK_EQ(object_view_box->GetType(), BasicShape::kBasicShapeInsetType);

  Path path;
  gfx::RectF bounding_box(0, 0, intrinsic_size.width.ToFloat(),
                          intrinsic_size.height.ToFloat());
  object_view_box->GetPath(path, bounding_box, 1.f);

  const PhysicalRect view_box_rect =
      PhysicalRect::EnclosingRect(path.BoundingRect());
  if (view_box_rect.IsEmpty())
    return std::nullopt;

  const PhysicalRect intrinsic_rect(PhysicalOffset(), intrinsic_size);
  if (view_box_rect == intrinsic_rect)
    return std::nullopt;

  return view_box_rect;
}

PhysicalRect LayoutReplaced::ComputeReplacedContentRect(
    const PhysicalRect& base_content_rect,
    const PhysicalSize* overridden_intrinsic_size) const {
  // |intrinsic_size| provides the size of the embedded content rendered in the
  // replaced element. This is the reference size that object-view-box applies
  // to.
  // If present, object-view-box changes the notion of embedded content used for
  // painting the element and applying rest of the object* properties. The
  // following cases are possible:
  //
  // - object-view-box is a subset of the embedded content. For example,
  // [0,0 50x50] on an image with bounds 100x100.
  //
  // - object-view-box is a superset of the embedded content. For example,
  // [-10, -10, 120x120] on an image with bounds 100x100.
  //
  // - object-view-box intersects with the embedded content. For example,
  // [-10, -10, 50x50] on an image with bounds 100x100.
  //
  // - object-view-box has no intersection with the embedded content. For
  // example, [-50, -50, 50x50] on any image.
  //
  // The image is scaled (by object-fit) and positioned (by object-position)
  // assuming the embedded content to be provided by the box identified by
  // object-view-box.
  //
  // Regions outside the image bounds (but within object-view-box) paint
  // transparent pixels. Regions outside object-view-box (but within image
  // bounds) are scaled as defined by object-fit above and treated as ink
  // overflow.
  const auto& intrinsic_size_for_object_view_box =
      overridden_intrinsic_size ? *overridden_intrinsic_size : intrinsic_size_;
  const auto view_box =
      ComputeObjectViewBoxRect(&intrinsic_size_for_object_view_box);

  // If no view box override was applied, then we don't need to adjust the
  // view-box paint rect.
  if (!view_box) {
    return ComputeObjectFitAndPositionRect(base_content_rect,
                                           overridden_intrinsic_size);
  }

  // Compute the paint rect based on bounds provided by the view box.
  DCHECK(!view_box->IsEmpty());
  const PhysicalSize view_box_size(view_box->Width(), view_box->Height());
  const auto view_box_paint_rect =
      ComputeObjectFitAndPositionRect(base_content_rect, &view_box_size);
  if (view_box_paint_rect.IsEmpty())
    return view_box_paint_rect;

  // Scale the original image bounds by the scale applied to the view box.
  auto scaled_width = intrinsic_size_for_object_view_box.width.MulDiv(
      view_box_paint_rect.Width(), view_box->Width());
  auto scaled_height = intrinsic_size_for_object_view_box.height.MulDiv(
      view_box_paint_rect.Height(), view_box->Height());
  const PhysicalSize scaled_image_size(scaled_width, scaled_height);

  // Scale the offset from the image origin by the scale applied to the view
  // box.
  auto scaled_x_offset =
      view_box->X().MulDiv(view_box_paint_rect.Width(), view_box->Width());
  auto scaled_y_offset =
      view_box->Y().MulDiv(view_box_paint_rect.Height(), view_box->Height());
  const PhysicalOffset scaled_offset(scaled_x_offset, scaled_y_offset);

  return PhysicalRect(view_box_paint_rect.offset - scaled_offset,
                      scaled_image_size);
}

PhysicalRect LayoutReplaced::ComputeObjectFitAndPositionRect(
    const PhysicalRect& base_content_rect,
    const PhysicalSize* overridden_intrinsic_size) const {
  NOT_DESTROYED();
  EObjectFit object_fit = StyleRef().GetObjectFit();

  if (object_fit == EObjectFit::kFill &&
      StyleRef().ObjectPosition() ==
          ComputedStyleInitialValues::InitialObjectPosition()) {
    return base_content_rect;
  }

  // TODO(davve): intrinsicSize doubles as both intrinsic size and intrinsic
  // ratio. In the case of SVG images this isn't correct since they can have
  // intrinsic ratio but no intrinsic size. In order to maintain aspect ratio,
  // the intrinsic size for SVG might be faked from the aspect ratio,
  // see SVGImage::containerSize().
  PhysicalSize intrinsic_size(
      overridden_intrinsic_size ? *overridden_intrinsic_size : IntrinsicSize());
  if (intrinsic_size.IsEmpty())
    return base_content_rect;

  PhysicalSize scaled_intrinsic_size(intrinsic_size);
  PhysicalRect final_rect = base_content_rect;
  switch (object_fit) {
    case EObjectFit::kScaleDown:
      // Srcset images have an intrinsic size depending on their destination,
      // but with object-fit: scale-down they need to use the underlying image
      // src's size. So revert back to the original size in that case.
      if (auto* image = DynamicTo<LayoutImage>(this)) {
        scaled_intrinsic_size.Scale(1.0 / image->ImageDevicePixelRatio());
      }
      [[fallthrough]];
    case EObjectFit::kContain:
    case EObjectFit::kCover:
      final_rect.size = final_rect.size.FitToAspectRatio(
          intrinsic_size, object_fit == EObjectFit::kCover
                              ? kAspectRatioFitGrow
                              : kAspectRatioFitShrink);
      if (object_fit != EObjectFit::kScaleDown ||
          final_rect.Width() <= scaled_intrinsic_size.width)
        break;
      [[fallthrough]];
    case EObjectFit::kNone:
      final_rect.size = scaled_intrinsic_size;
      break;
    case EObjectFit::kFill:
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  LayoutUnit x_offset =
      MinimumValueForLength(StyleRef().ObjectPosition().X(),
                            base_content_rect.Width() - final_rect.Width());
  LayoutUnit y_offset =
      MinimumValueForLength(StyleRef().ObjectPosition().Y(),
                            base_content_rect.Height() - final_rect.Height());
  final_rect.Move(PhysicalOffset(x_offset, y_offset));

  return final_rect;
}

PhysicalRect LayoutReplaced::ReplacedContentRect() const {
  NOT_DESTROYED();
  // This function should compute the result with old geometry even if a
  // BoxLayoutExtraInput exists.
  return ReplacedContentRectFrom(PhysicalContentBoxRect());
}

PhysicalRect LayoutReplaced::ReplacedContentRectFrom(
    const PhysicalRect& base_content_rect) const {
  NOT_DESTROYED();
  return ComputeReplacedContentRect(base_content_rect);
}

PhysicalRect LayoutReplaced::PreSnappedRectForPersistentSizing(
    const PhysicalRect& rect) {
  return PhysicalRect(rect.offset, PhysicalSize(ToRoundedSize(rect.size)));
}

void LayoutReplaced::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  DCHECK(!ShouldApplySizeContainment());

  if (auto view_box = ComputeObjectViewBoxRect()) {
    intrinsic_sizing_info.size = gfx::SizeF(view_box->size);
  } else {
    intrinsic_sizing_info.size = gfx::SizeF(IntrinsicSize());
  }

  // Figure out if we need to compute an intrinsic ratio.
  if (!LayoutObjectHasIntrinsicAspectRatio(this))
    return;

  if (!intrinsic_sizing_info.size.IsEmpty())
    intrinsic_sizing_info.aspect_ratio = intrinsic_sizing_info.size;
}

static std::pair<LayoutUnit, LayoutUnit> SelectionTopAndBottom(
    const LayoutReplaced& layout_replaced) {
  // TODO(layout-dev): This code is buggy if the replaced element is relative
  // positioned.

  // The fallback answer when we can't find the containing line box of
  // |layout_replaced|.
  const std::pair<LayoutUnit, LayoutUnit> fallback(
      layout_replaced.LogicalTop(), layout_replaced.LogicalBottom());

  if (layout_replaced.IsInline() &&
      layout_replaced.IsInLayoutNGInlineFormattingContext()) {
    // Step 1: Find the line box containing |layout_replaced|.
    InlineCursor line_box;
    line_box.MoveTo(layout_replaced);
    if (!line_box)
      return fallback;
    line_box.MoveToContainingLine();
    if (!line_box)
      return fallback;

    // Step 2: Return the logical top and bottom of the line box.
    // TODO(layout-dev): Use selection top & bottom instead of line's, or decide
    // if we still want to distinguish line and selection heights in NG.
    const ComputedStyle& line_style = line_box.Current().Style();
    const auto writing_direction = line_style.GetWritingDirection();
    const WritingModeConverter converter(writing_direction,
                                         line_box.ContainerFragment().Size());
    PhysicalRect physical_rect = line_box.Current().RectInContainerFragment();
    // The caller expects it to be in the "stitched" coordinate space.
    physical_rect.offset +=
        OffsetInStitchedFragments(line_box.ContainerFragment());
    const LogicalRect logical_rect = converter.ToLogical(physical_rect);
    return {logical_rect.offset.block_offset, logical_rect.BlockEndOffset()};
  }

  return fallback;
}

PositionWithAffinity LayoutReplaced::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();

  auto [top, bottom] = SelectionTopAndBottom(*this);

  LayoutUnit block_direction_position;
  LayoutUnit line_direction_position;
  if (RuntimeEnabledFeatures::SidewaysWritingModesEnabled()) {
    LogicalOffset logical_point =
        LocationContainer()->CreateWritingModeConverter().ToLogical(
            point + PhysicalLocation(), {});
    block_direction_position = logical_point.block_offset;
    line_direction_position = logical_point.inline_offset;
  } else {
    LayoutPoint flipped_point_in_container =
        LocationContainer()->FlipForWritingMode(point + PhysicalLocation());
    block_direction_position = IsHorizontalWritingMode()
                                   ? flipped_point_in_container.Y()
                                   : flipped_point_in_container.X();
    line_direction_position = IsHorizontalWritingMode()
                                  ? flipped_point_in_container.X()
                                  : flipped_point_in_container.Y();
  }

  if (block_direction_position < top)
    return PositionBeforeThis();  // coordinates are above

  if (block_direction_position >= bottom)
    return PositionBeforeThis();  // coordinates are below

  if (GetNode()) {
    const bool is_at_left_side =
        line_direction_position <= LogicalLeft() + (LogicalWidth() / 2);
    const bool is_at_start = is_at_left_side == IsLtr(ResolvedDirection());
    if (is_at_start)
      return PositionBeforeThis();
    return PositionAfterThis();
  }

  return LayoutBox::PositionForPoint(point);
}

PhysicalRect LayoutReplaced::LocalSelectionVisualRect() const {
  NOT_DESTROYED();
  if (GetSelectionState() == SelectionState::kNone ||
      GetSelectionState() == SelectionState::kContain) {
    return PhysicalRect();
  }

  if (IsInline() && IsInLayoutNGInlineFormattingContext()) {
    PhysicalRect rect;
    InlineCursor cursor;
    cursor.MoveTo(*this);
    for (; cursor; cursor.MoveToNextForSameLayoutObject())
      rect.Unite(cursor.CurrentLocalSelectionRectForReplaced());
    return rect;
  }

  // We're a block-level replaced element.  Just return our own dimensions.
  return PhysicalRect(PhysicalOffset(), Size());
}

bool LayoutReplaced::RespectsCSSOverflow() const {
  const Element* element = DynamicTo<Element>(GetNode());
  return element && element->IsReplacedElementRespectingCSSOverflow();
}

bool LayoutReplaced::ClipsToContentBox() const {
  if (!RespectsCSSOverflow()) {
    // If an svg is clipped, it is guaranteed to be clipped to the element's
    // content box.
    if (IsSVGRoot())
      return GetOverflowClipAxes() == kOverflowClipBothAxis;
    return true;
  }

  // TODO(khushalsagar): There can be more cases where the content clips to
  // content box. For instance, when padding is 0 and the reference box is the
  // padding box.
  const auto& overflow_clip_margin = StyleRef().OverflowClipMargin();
  return GetOverflowClipAxes() == kOverflowClipBothAxis &&
         overflow_clip_margin &&
         overflow_clip_margin->GetReferenceBox() ==
             StyleOverflowClipMargin::ReferenceBox::kContentBox &&
         !overflow_clip_margin->GetMargin();
}

}  // namespace blink
