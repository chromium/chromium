/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"

#include <memory>

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

gfx::Rect ToPixelSnappedLogicalRect(const LogicalRect& rect) {
  return gfx::Rect(
      rect.offset.inline_offset.Round(), rect.offset.block_offset.Round(),
      SnapSizeToPixel(rect.size.inline_size, rect.offset.inline_offset),
      SnapSizeToPixel(rect.size.block_size, rect.offset.block_offset));
}

// Unlike LayoutBoxModelObject::PhysicalBorderToLogical(), this function
// applies container's WritingDirectionMode.
PhysicalToLogicalGetter<LayoutUnit, LayoutBox> LogicalBorder(
    const LayoutBox& layout_box,
    const ComputedStyle& container_style) {
  return PhysicalToLogicalGetter<LayoutUnit, LayoutBox>(
      container_style.GetWritingDirection(), layout_box, &LayoutBox::BorderTop,
      &LayoutBox::BorderRight, &LayoutBox::BorderBottom,
      &LayoutBox::BorderLeft);
}

// Unlike LayoutBoxModelObject::PhysicalPaddingToLogical(), this function
// applies container's WritingDirectionMode.
PhysicalToLogicalGetter<LayoutUnit, LayoutBox> LogicalPadding(
    const LayoutBox& layout_box,
    const ComputedStyle& container_style) {
  return PhysicalToLogicalGetter<LayoutUnit, LayoutBox>(
      container_style.GetWritingDirection(), layout_box, &LayoutBox::PaddingTop,
      &LayoutBox::PaddingRight, &LayoutBox::PaddingBottom,
      &LayoutBox::PaddingLeft);
}

}  // namespace

CSSBoxType ReferenceBox(const ShapeValue& shape_value) {
  if (shape_value.CssBox() == CSSBoxType::kMissing)
    return CSSBoxType::kMargin;
  return shape_value.CssBox();
}

void ShapeOutsideInfo::SetReferenceBoxLogicalSize(
    LogicalSize new_reference_box_logical_size,
    LogicalSize margin_size) {
  Document& document = layout_box_->GetDocument();
  bool is_horizontal_writing_mode =
      layout_box_->ContainingBlock()->StyleRef().IsHorizontalWritingMode();

  LogicalSize margin_box_for_use_counter = new_reference_box_logical_size;
  margin_box_for_use_counter.Expand(margin_size.inline_size,
                                    margin_size.block_size);

  const ShapeValue& shape_value = *layout_box_->StyleRef().ShapeOutside();
  switch (ReferenceBox(shape_value)) {
    case CSSBoxType::kMargin:
      UseCounter::Count(document, WebFeature::kShapeOutsideMarginBox);
      new_reference_box_logical_size.Expand(margin_size.inline_size,
                                            margin_size.block_size);
      break;
    case CSSBoxType::kBorder:
      UseCounter::Count(document, WebFeature::kShapeOutsideBorderBox);
      break;
    case CSSBoxType::kPadding:
      UseCounter::Count(document, WebFeature::kShapeOutsidePaddingBox);
      if (is_horizontal_writing_mode) {
        new_reference_box_logical_size.Shrink(layout_box_->BorderWidth(),
                                              layout_box_->BorderHeight());
      } else {
        new_reference_box_logical_size.Shrink(layout_box_->BorderHeight(),
                                              layout_box_->BorderWidth());
      }

      if (new_reference_box_logical_size != margin_box_for_use_counter) {
        UseCounter::Count(
            document,
            WebFeature::kShapeOutsidePaddingBoxDifferentFromMarginBox);
      }
      break;
    case CSSBoxType::kContent: {
      bool is_shape_image = shape_value.GetType() == ShapeValue::kImage;

      if (!is_shape_image)
        UseCounter::Count(document, WebFeature::kShapeOutsideContentBox);

      if (is_horizontal_writing_mode) {
        new_reference_box_logical_size.Shrink(
            layout_box_->BorderAndPaddingWidth(),
            layout_box_->BorderAndPaddingHeight());
      } else {
        new_reference_box_logical_size.Shrink(
            layout_box_->BorderAndPaddingHeight(),
            layout_box_->BorderAndPaddingWidth());
      }

      if (!is_shape_image &&
          new_reference_box_logical_size != margin_box_for_use_counter) {
        UseCounter::Count(
            document,
            WebFeature::kShapeOutsideContentBoxDifferentFromMarginBox);
      }
      break;
    }
    case CSSBoxType::kMissing:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  new_reference_box_logical_size.ClampNegativeToZero();

  if (reference_box_logical_size_ == new_reference_box_logical_size)
    return;
  MarkShapeAsDirty();
  reference_box_logical_size_ = new_reference_box_logical_size;
}

void ShapeOutsideInfo::SetPercentageResolutionInlineSize(
    LayoutUnit percentage_resolution_inline_size) {
  if (percentage_resolution_inline_size_ == percentage_resolution_inline_size)
    return;

  MarkShapeAsDirty();
  percentage_resolution_inline_size_ = percentage_resolution_inline_size;
}

static bool CheckShapeImageOrigin(Document& document,
                                  const StyleImage& style_image) {
  String failing_url;
  if (style_image.IsAccessAllowed(failing_url))
    return true;
  String url_string = failing_url.IsNull() ? "''" : failing_url;
  document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError,
      "Unsafe attempt to load URL " + url_string + "."));
  return false;
}

static PhysicalRect GetShapeImagePhysicalMarginRect(
    const LayoutBox& layout_box,
    const PhysicalSize& reference_physical_size) {
  PhysicalBoxStrut margin_border_padding = layout_box.MarginBoxOutsets() +
                                           layout_box.BorderOutsets() +
                                           layout_box.PaddingOutsets();
  return PhysicalRect(
      -margin_border_padding.left, -margin_border_padding.top,
      margin_border_padding.HorizontalSum() + reference_physical_size.width,
      margin_border_padding.VerticalSum() + reference_physical_size.height);
}

static LogicalRect GetShapeImageMarginRect(
    const LayoutBox& layout_box,
    const LogicalSize& reference_box_logical_size) {
  BoxStrut outsets =
      (layout_box.MarginBoxOutsets() + layout_box.BorderOutsets() +
       layout_box.PaddingOutsets())
          .ConvertToLogical(layout_box.Style()->GetWritingDirection());
  LogicalOffset margin_box_origin(-outsets.inline_start, -outsets.block_start);
  LogicalSize margin_rect_size = reference_box_logical_size;
  margin_rect_size.Expand(outsets.InlineSum(), outsets.BlockSum());
  margin_rect_size.ClampNegativeToZero();
  return LogicalRect(margin_box_origin, margin_rect_size);
}

PhysicalSize ShapeOutsideInfo::ReferenceBoxPhysicalSize() const {
  return ToPhysicalSize(
      reference_box_logical_size_,
      layout_box_->ContainingBlock()->Style()->GetWritingMode());
}

std::unique_ptr<Shape> ShapeOutsideInfo::CreateShapeForImage(
    StyleImage* style_image,
    float shape_image_threshold,
    WritingMode writing_mode,
    float margin) const {
  DCHECK(!style_image->IsPendingImage());

  PhysicalSize reference_physical_size = ReferenceBoxPhysicalSize();
  RespectImageOrientationEnum respect_orientation =
      style_image->ForceOrientationIfNecessary(
          layout_box_->StyleRef().ImageOrientation());

  const gfx::SizeF image_size = style_image->ImageSize(
      layout_box_->StyleRef().EffectiveZoom(),
      RuntimeEnabledFeatures::ShapeOutsideWritingModeFixEnabled()
          ? gfx::SizeF(reference_physical_size)
          : gfx::SizeF(reference_box_logical_size_.inline_size.ToFloat(),
                       reference_box_logical_size_.block_size.ToFloat()),
      respect_orientation);

  LogicalRect margin_rect;
  if (RuntimeEnabledFeatures::ShapeOutsideWritingModeFixEnabled()) {
    WritingModeConverter converter({writing_mode, TextDirection::kLtr},
                                   reference_physical_size);
    margin_rect = converter.ToLogical(
        GetShapeImagePhysicalMarginRect(*layout_box_, reference_physical_size));
    margin_rect.size.inline_size =
        margin_rect.size.inline_size.ClampNegativeToZero();
    margin_rect.size.block_size =
        margin_rect.size.block_size.ClampNegativeToZero();
  } else {
    margin_rect =
        GetShapeImageMarginRect(*layout_box_, reference_box_logical_size_);
  }

  gfx::Rect image_rect;
  const PhysicalRect image_physical_rect =
      layout_box_->IsLayoutImage()
          ? To<LayoutImage>(layout_box_.Get())->ReplacedContentRect()
          : PhysicalRect({}, PhysicalSize::FromSizeFRound(image_size));
  if (RuntimeEnabledFeatures::ShapeOutsideWritingModeFixEnabled()) {
    WritingModeConverter converter({writing_mode, TextDirection::kLtr},
                                   reference_physical_size);
    image_rect =
        ToPixelSnappedLogicalRect(converter.ToLogical(image_physical_rect));
  } else {
    image_rect = ToPixelSnappedRect(image_physical_rect);
  }

  scoped_refptr<Image> image =
      style_image->GetImage(*layout_box_, layout_box_->GetDocument(),
                            layout_box_->StyleRef(), image_size);

  return Shape::CreateRasterShape(
      image.get(), shape_image_threshold,
      reference_box_logical_size_.block_size.Floor(), image_rect,
      ToPixelSnappedLogicalRect(margin_rect), writing_mode, margin,
      respect_orientation);
}

const Shape& ShapeOutsideInfo::ComputedShape() const {
  if (Shape* shape = shape_.get())
    return *shape;

  base::AutoReset<bool> is_in_computing_shape(&is_computing_shape_, true);

  const ComputedStyle& style = *layout_box_->Style();
  DCHECK(layout_box_->ContainingBlock());
  const LayoutBlock& containing_block = *layout_box_->ContainingBlock();
  const ComputedStyle& containing_block_style = containing_block.StyleRef();

  WritingMode writing_mode = containing_block_style.GetWritingMode();
  // Make sure contentWidth is not negative. This can happen when containing
  // block has a vertical scrollbar and its content is smaller than the
  // scrollbar width.
  LayoutUnit percentage_resolution_inline_size =
      containing_block.IsLayoutNGObject()
          ? percentage_resolution_inline_size_
          : std::max(LayoutUnit(), containing_block.ContentWidth());

  float margin =
      FloatValueForLength(layout_box_->StyleRef().ShapeMargin(),
                          percentage_resolution_inline_size.ToFloat());

  float shape_image_threshold = style.ShapeImageThreshold();
  DCHECK(style.ShapeOutside());
  const ShapeValue& shape_value = *style.ShapeOutside();

  switch (shape_value.GetType()) {
    case ShapeValue::kShape:
      DCHECK(shape_value.Shape());
      shape_ =
          Shape::CreateShape(shape_value.Shape(), reference_box_logical_size_,
                             writing_mode, margin);
      break;
    case ShapeValue::kImage:
      DCHECK(shape_value.GetImage());
      DCHECK(shape_value.GetImage()->IsLoaded());
      DCHECK(shape_value.GetImage()->CanRender());
      shape_ = CreateShapeForImage(shape_value.GetImage(),
                                   shape_image_threshold, writing_mode, margin);
      break;
    case ShapeValue::kBox: {
      // TODO(layout-dev): It seems incorrect to pass logical size to
      // RoundedBorderGeometry().
      PhysicalSize size =
          RuntimeEnabledFeatures::ShapeOutsideWritingModeFixEnabled()
              ? ReferenceBoxPhysicalSize()
              : PhysicalSize(reference_box_logical_size_.inline_size,
                             reference_box_logical_size_.block_size);
      const FloatRoundedRect& shape_rect = RoundedBorderGeometry::RoundedBorder(
          style, PhysicalRect(PhysicalOffset(), size));
      shape_ = Shape::CreateLayoutBoxShape(shape_rect, writing_mode, margin);
      break;
    }
  }

  DCHECK(shape_);
  return *shape_;
}

LayoutUnit ShapeOutsideInfo::BlockStartOffset() const {
  const ComputedStyle& container_style =
      layout_box_->ContainingBlock()->StyleRef();
  switch (ReferenceBox(*layout_box_->StyleRef().ShapeOutside())) {
    case CSSBoxType::kMargin:
      return -layout_box_->MarginBlockStart(&container_style);
    case CSSBoxType::kBorder:
      return LayoutUnit();
    case CSSBoxType::kPadding:
      return LogicalBorder(*layout_box_, container_style).BlockStart();
    case CSSBoxType::kContent:
      return LogicalBorder(*layout_box_, container_style).BlockStart() +
             LogicalPadding(*layout_box_, container_style).BlockStart();
    case CSSBoxType::kMissing:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return LayoutUnit();
}

LayoutUnit ShapeOutsideInfo::InlineStartOffset() const {
  const ComputedStyle& container_style =
      layout_box_->ContainingBlock()->StyleRef();
  switch (ReferenceBox(*layout_box_->StyleRef().ShapeOutside())) {
    case CSSBoxType::kMargin:
      return -layout_box_->MarginInlineStart(&container_style);
    case CSSBoxType::kBorder:
      return LayoutUnit();
    case CSSBoxType::kPadding:
      return LogicalBorder(*layout_box_, container_style).InlineStart();
    case CSSBoxType::kContent:
      return LogicalBorder(*layout_box_, container_style).InlineStart() +
             LogicalPadding(*layout_box_, container_style).InlineStart();
    case CSSBoxType::kMissing:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return LayoutUnit();
}

bool ShapeOutsideInfo::IsEnabledFor(const LayoutBox& box) {
  ShapeValue* shape_value = box.StyleRef().ShapeOutside();
  if (!box.IsFloating() || !shape_value)
    return false;

  switch (shape_value->GetType()) {
    case ShapeValue::kShape:
      return shape_value->Shape();
    case ShapeValue::kImage: {
      StyleImage* image = shape_value->GetImage();
      DCHECK(image);
      return image->IsLoaded() && image->CanRender() &&
             CheckShapeImageOrigin(box.GetDocument(), *image);
    }
    case ShapeValue::kBox:
      return true;
  }

  return false;
}

PhysicalRect ShapeOutsideInfo::ComputedShapePhysicalBoundingBox() const {
  LogicalRect logical_box = ComputedShape().ShapeMarginLogicalBoundingBox();
  // TODO(crbug.com/1463823): The logic of this function looks incorrect.
  PhysicalRect physical_bounding_box(
      logical_box.offset.inline_offset, logical_box.offset.block_offset,
      logical_box.size.inline_size, logical_box.size.block_size);
  physical_bounding_box.offset.left += InlineStartOffset();

  if (layout_box_->StyleRef().IsFlippedBlocksWritingMode()) {
    physical_bounding_box.offset.top =
        layout_box_->LogicalHeight() - physical_bounding_box.Bottom();
  } else {
    physical_bounding_box.offset.top += BlockStartOffset();
  }

  if (!layout_box_->StyleRef().IsHorizontalWritingMode()) {
    physical_bounding_box = PhysicalRect(
        physical_bounding_box.offset.top, physical_bounding_box.offset.left,
        physical_bounding_box.size.height, physical_bounding_box.size.width);
  } else {
    physical_bounding_box.offset.top += BlockStartOffset();
  }

  return physical_bounding_box;
}

gfx::PointF ShapeOutsideInfo::ShapeToLayoutObjectPoint(
    gfx::PointF point) const {
  gfx::PointF result = gfx::PointF(point.x() + InlineStartOffset(),
                                   point.y() + BlockStartOffset());
  if (layout_box_->StyleRef().IsFlippedBlocksWritingMode())
    result.set_y(layout_box_->LogicalHeight() - result.y());
  if (!layout_box_->StyleRef().IsHorizontalWritingMode())
    result.Transpose();
  return result;
}

// static
ShapeOutsideInfo::InfoMap& ShapeOutsideInfo::GetInfoMap() {
  DEFINE_STATIC_LOCAL(Persistent<InfoMap>, static_info_map,
                      (MakeGarbageCollected<InfoMap>()));
  return *static_info_map;
}

void ShapeOutsideInfo::Trace(Visitor* visitor) const {
  visitor->Trace(layout_box_);
}

}  // namespace blink
