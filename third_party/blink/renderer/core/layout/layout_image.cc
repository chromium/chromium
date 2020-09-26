/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010, 2011 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/layout_image.h"

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/media_element_parser_helpers.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/image_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

LayoutImage::LayoutImage(Element* element)
    : LayoutReplaced(element, LayoutSize()),
      did_increment_visually_non_empty_pixel_count_(false),
      is_generated_content_(false),
      image_device_pixel_ratio_(1.0f) {}

LayoutImage* LayoutImage::CreateAnonymous(PseudoElement& pseudo) {
  LayoutImage* image = new LayoutImage(nullptr);
  image->SetDocumentForAnonymous(&pseudo.GetDocument());
  return image;
}

LayoutImage::~LayoutImage() = default;

void LayoutImage::WillBeDestroyed() {
  NOT_DESTROYED();
  DCHECK(image_resource_);
  image_resource_->Shutdown();

  LayoutReplaced::WillBeDestroyed();
}

void LayoutImage::StyleDidChange(StyleDifference diff,
                                 const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutReplaced::StyleDidChange(diff, old_style);

  bool old_orientation =
      old_style ? old_style->RespectImageOrientation()
                : ComputedStyleInitialValues::InitialRespectImageOrientation();
  if (Style() && StyleRef().RespectImageOrientation() != old_orientation)
    IntrinsicSizeChanged();
}

void LayoutImage::SetImageResource(LayoutImageResource* image_resource) {
  NOT_DESTROYED();
  DCHECK(!image_resource_);
  image_resource_ = image_resource;
  image_resource_->Initialize(this);
}

void LayoutImage::ImageChanged(WrappedImagePtr new_image,
                               CanDeferInvalidation defer) {
  NOT_DESTROYED();
  DCHECK(View());
  DCHECK(View()->GetFrameView());
  if (DocumentBeingDestroyed())
    return;

  if (HasBoxDecorationBackground() || HasMask() || HasShapeOutside() ||
      HasReflection())
    LayoutReplaced::ImageChanged(new_image, defer);

  if (!image_resource_)
    return;

  if (new_image != image_resource_->ImagePtr())
    return;

  auto* html_image_element = DynamicTo<HTMLImageElement>(GetNode());
  if (IsGeneratedContent() && html_image_element &&
      image_resource_->ErrorOccurred()) {
    html_image_element->EnsureFallbackForGeneratedContent();
    return;
  }

  // If error occurred, image marker should be replaced by a LayoutText.
  // NotifyOfSubtreeChange to make list item updating its marker content.
  if (IsListMarkerImage() && image_resource_->ErrorOccurred())
    NotifyOfSubtreeChange();

  // Per the spec, we let the server-sent header override srcset/other sources
  // of dpr.
  // https://github.com/igrigorik/http-client-hints/blob/master/draft-grigorik-http-client-hints-01.txt#L255
  if (image_resource_->CachedImage() &&
      image_resource_->CachedImage()->HasDevicePixelRatioHeaderValue()) {
    UseCounter::Count(GetDocument(), WebFeature::kClientHintsContentDPR);
    image_device_pixel_ratio_ =
        1 / image_resource_->CachedImage()->DevicePixelRatioHeaderValue();
  }

  if (!did_increment_visually_non_empty_pixel_count_) {
    // At a zoom level of 1 the image is guaranteed to have an integer size.
    View()->GetFrameView()->IncrementVisuallyNonEmptyPixelCount(
        FlooredIntSize(ImageSizeOverriddenByIntrinsicSize(1.0f)));
    did_increment_visually_non_empty_pixel_count_ = true;
  }

  // The replaced content transform depends on the intrinsic size (see:
  // FragmentPaintPropertyTreeBuilder::UpdateReplacedContentTransform).
  SetNeedsPaintPropertyUpdate();
  InvalidatePaintAndMarkForLayoutIfNeeded(defer);
}

void LayoutImage::UpdateIntrinsicSizeIfNeeded(const LayoutSize& new_size) {
  NOT_DESTROYED();
  if (image_resource_->ErrorOccurred())
    return;
  SetIntrinsicSize(new_size);
}

bool LayoutImage::NeedsLayoutOnIntrinsicSizeChange() const {
  NOT_DESTROYED();
  // If the actual area occupied by the image has changed and it is not
  // constrained by style then a layout is required.
  bool image_size_is_constrained =
      StyleRef().LogicalWidth().IsSpecified() &&
      StyleRef().LogicalHeight().IsSpecified() &&
      !HasAutoHeightOrContainingBlockWithAutoHeight(
          kDontRegisterPercentageDescendant);
  if (!image_size_is_constrained)
    return true;
  // Flex layout algorithm uses the intrinsic image width/height even if
  // width/height are specified.
  if (IsFlexItemIncludingNG())
    return true;
  // FIXME: We only need to recompute the containing block's preferred size if
  // the containing block's size depends on the image's size (i.e., the
  // container uses shrink-to-fit sizing). There's no easy way to detect that
  // shrink-to-fit is needed, always force a layout.
  return HasRelativeLogicalWidth();
}

void LayoutImage::InvalidatePaintAndMarkForLayoutIfNeeded(
    CanDeferInvalidation defer) {
  NOT_DESTROYED();
  LayoutSize old_intrinsic_size = IntrinsicSize();

  LayoutSize new_intrinsic_size = RoundedLayoutSize(
      ImageSizeOverriddenByIntrinsicSize(StyleRef().EffectiveZoom()));
  UpdateIntrinsicSizeIfNeeded(new_intrinsic_size);

  // In the case of generated image content using :before/:after/content, we
  // might not be in the layout tree yet. In that case, we just need to update
  // our intrinsic size. layout() will be called after we are inserted in the
  // tree which will take care of what we are doing here.
  if (!ContainingBlock())
    return;

  if (old_intrinsic_size != new_intrinsic_size) {
    SetIntrinsicLogicalWidthsDirty();

    if (NeedsLayoutOnIntrinsicSizeChange()) {
      SetNeedsLayoutAndFullPaintInvalidation(
          layout_invalidation_reason::kSizeChanged);
      return;
    }
  }

  SetShouldDoFullPaintInvalidationWithoutGeometryChange(
      PaintInvalidationReason::kImage);

  if (defer == CanDeferInvalidation::kYes && ImageResource() &&
      ImageResource()->MaybeAnimated())
    SetShouldDelayFullPaintInvalidation();

  // Tell any potential compositing layers that the image needs updating.
  ContentChanged(kImageChanged);
}

void LayoutImage::ImageNotifyFinished(ImageResourceContent* new_image) {
  NOT_DESTROYED();
  LayoutObject::ImageNotifyFinished(new_image);
  if (!image_resource_)
    return;

  if (DocumentBeingDestroyed())
    return;

  if (new_image == image_resource_->CachedImage()) {
    // tell any potential compositing layers
    // that the image is done and they can reference it directly.
    ContentChanged(kImageChanged);
  }
}

void LayoutImage::PaintReplaced(const PaintInfo& paint_info,
                                const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  if (ChildPaintBlockedByDisplayLock())
    return;
  ImagePainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutImage::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  ImagePainter(*this).Paint(paint_info);
}

void LayoutImage::AreaElementFocusChanged(HTMLAreaElement* area_element) {
  NOT_DESTROYED();
  DCHECK_EQ(area_element->ImageElement(), GetNode());

  if (area_element->GetPath(this).IsEmpty())
    return;

  InvalidatePaintAndMarkForLayoutIfNeeded(CanDeferInvalidation::kYes);
}

bool LayoutImage::ForegroundIsKnownToBeOpaqueInRect(
    const PhysicalRect& local_rect,
    unsigned) const {
  NOT_DESTROYED();
  if (!image_resource_->HasImage() || image_resource_->ErrorOccurred())
    return false;
  ImageResourceContent* image_content = image_resource_->CachedImage();
  if (!image_content || !image_content->IsLoaded())
    return false;
  if (!PhysicalContentBoxRect().Contains(local_rect))
    return false;
  EFillBox background_clip = StyleRef().BackgroundClip();
  // Background paints under borders.
  if (background_clip == EFillBox::kBorder && StyleRef().HasBorder() &&
      !StyleRef().BorderObscuresBackground())
    return false;
  // Background shows in padding area.
  if ((background_clip == EFillBox::kBorder ||
       background_clip == EFillBox::kPadding) &&
      StyleRef().MayHavePadding())
    return false;
  // Object-position may leave parts of the content box empty, regardless of the
  // value of object-fit.
  if (StyleRef().ObjectPosition() !=
      ComputedStyleInitialValues::InitialObjectPosition())
    return false;
  // Object-fit may leave parts of the content box empty.
  EObjectFit object_fit = StyleRef().GetObjectFit();
  if (object_fit != EObjectFit::kFill && object_fit != EObjectFit::kCover)
    return false;
  // Check for image with alpha.
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
               "data", inspector_paint_image_event::Data(this, *image_content));
  return image_content->GetImage()->CurrentFrameKnownToBeOpaque();
}

bool LayoutImage::ComputeBackgroundIsKnownToBeObscured() const {
  NOT_DESTROYED();
  if (!StyleRef().HasBackground())
    return false;

  PhysicalRect painted_extent;
  if (!GetBackgroundPaintedExtent(painted_extent))
    return false;
  return ForegroundIsKnownToBeOpaqueInRect(painted_extent, 0);
}

LayoutUnit LayoutImage::MinimumReplacedHeight() const {
  NOT_DESTROYED();
  return image_resource_->ErrorOccurred() ? IntrinsicSize().Height()
                                          : LayoutUnit();
}

HTMLMapElement* LayoutImage::ImageMap() const {
  NOT_DESTROYED();
  auto* i = DynamicTo<HTMLImageElement>(GetNode());
  return i ? i->GetTreeScope().GetImageMap(
                 i->FastGetAttribute(html_names::kUsemapAttr))
           : nullptr;
}

bool LayoutImage::NodeAtPoint(HitTestResult& result,
                              const HitTestLocation& hit_test_location,
                              const PhysicalOffset& accumulated_offset,
                              HitTestAction hit_test_action) {
  NOT_DESTROYED();
  HitTestResult temp_result(result);
  bool inside = LayoutReplaced::NodeAtPoint(
      temp_result, hit_test_location, accumulated_offset, hit_test_action);

  if (!inside && result.GetHitTestRequest().ListBased())
    result.Append(temp_result);
  if (inside)
    result = temp_result;
  return inside;
}

bool LayoutImage::HasOverriddenIntrinsicSize() const {
  NOT_DESTROYED();
  if (!RuntimeEnabledFeatures::ExperimentalProductivityFeaturesEnabled())
    return false;
  auto* image_element = DynamicTo<HTMLImageElement>(GetNode());
  return image_element && image_element->IsDefaultIntrinsicSize();
}

FloatSize LayoutImage::ImageSizeOverriddenByIntrinsicSize(
    float multiplier) const {
  NOT_DESTROYED();
  if (!HasOverriddenIntrinsicSize())
    return image_resource_->ImageSize(multiplier);

  FloatSize overridden_intrinsic_size(kDefaultWidth, kDefaultHeight);
  if (multiplier != 1) {
    overridden_intrinsic_size.Scale(multiplier);
    if (overridden_intrinsic_size.Width() < 1.0f)
      overridden_intrinsic_size.SetWidth(1.0f);
    if (overridden_intrinsic_size.Height() < 1.0f)
      overridden_intrinsic_size.SetHeight(1.0f);
  }

  return overridden_intrinsic_size;
}

bool LayoutImage::OverrideIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  if (!HasOverriddenIntrinsicSize())
    return false;

  FloatSize overridden_intrinsic_size(kDefaultWidth, kDefaultHeight);
  intrinsic_sizing_info.size = overridden_intrinsic_size;
  intrinsic_sizing_info.aspect_ratio = intrinsic_sizing_info.size;
  if (!IsHorizontalWritingMode())
    intrinsic_sizing_info.Transpose();

  return true;
}

void LayoutImage::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  DCHECK(!ShouldApplySizeContainment());
  if (!OverrideIntrinsicSizingInfo(intrinsic_sizing_info)) {
    if (SVGImage* svg_image = EmbeddedSVGImage()) {
      svg_image->GetIntrinsicSizingInfo(intrinsic_sizing_info);

      // Handle zoom & vertical writing modes here, as the embedded SVG document
      // doesn't know about them.
      intrinsic_sizing_info.size.Scale(StyleRef().EffectiveZoom());
      if (StyleRef().GetObjectFit() != EObjectFit::kScaleDown)
        intrinsic_sizing_info.size.Scale(ImageDevicePixelRatio());

      // Handle an overridden aspect ratio
      if (const base::Optional<IntSize>& aspect_ratio =
              StyleRef().AspectRatio()) {
        intrinsic_sizing_info.aspect_ratio.SetWidth(aspect_ratio->Width());
        intrinsic_sizing_info.aspect_ratio.SetHeight(aspect_ratio->Height());
      }

      if (!IsHorizontalWritingMode())
        intrinsic_sizing_info.Transpose();
      return;
    }

    LayoutReplaced::ComputeIntrinsicSizingInfo(intrinsic_sizing_info);

    // Our intrinsicSize is empty if we're laying out generated images with
    // relative width/height. Figure out the right intrinsic size to use.
    if (intrinsic_sizing_info.size.IsEmpty() &&
        !image_resource_->HasIntrinsicSize() && !IsListMarkerImage()) {
      if (HasOverrideContainingBlockContentLogicalWidth() &&
          HasOverrideContainingBlockContentLogicalHeight()) {
        intrinsic_sizing_info.size.SetWidth(
            OverrideContainingBlockContentLogicalWidth().ToFloat());
        intrinsic_sizing_info.size.SetHeight(
            OverrideContainingBlockContentLogicalHeight().ToFloat());
      } else {
        LayoutObject* containing_block =
            IsOutOfFlowPositioned() ? Container() : ContainingBlock();
        if (containing_block->IsBox()) {
          LayoutBox* box = ToLayoutBox(containing_block);
          intrinsic_sizing_info.size.SetWidth(
              box->AvailableLogicalWidth().ToFloat());
          intrinsic_sizing_info.size.SetHeight(
              box->AvailableLogicalHeight(kIncludeMarginBorderPadding)
                  .ToFloat());
        }
      }
    }
  }
  // Don't compute an intrinsic ratio to preserve historical WebKit behavior if
  // we're painting alt text and/or a broken image.
  // Video is excluded from this behavior because video elements have a default
  // aspect ratio that a failed poster image load should not override.
  if (image_resource_ && image_resource_->ErrorOccurred() &&
      !IsA<LayoutVideo>(this)) {
    intrinsic_sizing_info.aspect_ratio = FloatSize(1, 1);
    return;
  }
}

bool LayoutImage::NeedsPreferredWidthsRecalculation() const {
  NOT_DESTROYED();
  if (LayoutReplaced::NeedsPreferredWidthsRecalculation())
    return true;
  SVGImage* svg_image = EmbeddedSVGImage();
  return svg_image && svg_image->HasIntrinsicSizingInfo();
}

SVGImage* LayoutImage::EmbeddedSVGImage() const {
  NOT_DESTROYED();
  if (!image_resource_)
    return nullptr;
  ImageResourceContent* cached_image = image_resource_->CachedImage();
  // TODO(japhet): This shouldn't need to worry about cache validation.
  // https://crbug.com/761026
  if (!cached_image || cached_image->IsCacheValidator())
    return nullptr;
  return DynamicTo<SVGImage>(cached_image->GetImage());
}

void LayoutImage::UpdateAfterLayout() {
  NOT_DESTROYED();
  LayoutBox::UpdateAfterLayout();
  Node* node = GetNode();
  if (auto* image_element = DynamicTo<HTMLImageElement>(node)) {
    media_element_parser_helpers::CheckUnsizedMediaViolation(
        this, image_element->IsDefaultIntrinsicSize());
  } else if (auto* video_element = DynamicTo<HTMLVideoElement>(node)) {
    media_element_parser_helpers::CheckUnsizedMediaViolation(
        this, video_element->IsDefaultIntrinsicSize());
  }
}

}  // namespace blink
