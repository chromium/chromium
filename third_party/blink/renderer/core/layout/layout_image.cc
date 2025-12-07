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

#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/image_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace blink {

LayoutImage::LayoutImage(Element* element) : LayoutReplaced(element) {}

LayoutImage* LayoutImage::CreateAnonymous(Document& document) {
  LayoutImage* image = MakeGarbageCollected<LayoutImage>(nullptr);
  image->SetDocumentForAnonymous(&document);
  return image;
}

LayoutImage::~LayoutImage() = default;

void LayoutImage::Trace(Visitor* visitor) const {
  visitor->Trace(image_resource_);
  LayoutReplaced::Trace(visitor);
}

void LayoutImage::WillBeDestroyed() {
  NOT_DESTROYED();
  DCHECK(image_resource_);
  image_resource_->Shutdown();

  LayoutReplaced::WillBeDestroyed();
}

void GetImageSizeChangeTracingData(perfetto::TracedValue context,
                                   Node* node,
                                   LocalFrame* frame) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("nodeId", IdentifiersFactory::IntIdForNode(node));
  dict.Add("frameId", IdentifiersFactory::FrameId(frame));
}

void LayoutImage::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const StyleChangeContext& style_change_context) {
  NOT_DESTROYED();
  LayoutReplaced::StyleDidChange(diff, old_style, style_change_context);

  RespectImageOrientationEnum old_orientation =
      old_style ? old_style->ImageOrientation()
                : ComputedStyleInitialValues::InitialImageOrientation();
  if (StyleRef().ImageOrientation() != old_orientation) {
    NaturalSizeChanged();
  }

  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), &tracing_enabled);

  if (tracing_enabled) {
    bool is_unsized = this->IsUnsizedImage();
    if (is_unsized) {
      Node* node = GetNode();
      TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
          "devtools.timeline", "LayoutImageUnsized", TRACE_EVENT_SCOPE_THREAD,
          base::TimeTicks::Now(), "data", [&](perfetto::TracedValue ctx) {
            GetImageSizeChangeTracingData(std::move(ctx), node, GetFrame());
          });
    }
  }
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
  if (IsListMarkerImage() && image_resource_->ErrorOccurred()) {
    LayoutObject* item = this;
    while (item->IsAnonymous())
      item = item->Parent();
    DCHECK(item);
    if (item->NotifyOfSubtreeChange())
      item->GetNode()->MarkAncestorsWithChildNeedsStyleRecalc();
  }

  // Per the spec, we let the server-sent header override srcset/other sources
  // of dpr.
  // https://github.com/igrigorik/http-client-hints/blob/master/draft-grigorik-http-client-hints-01.txt#L255
  if (image_resource_->CachedImage() &&
      image_resource_->CachedImage()->HasDevicePixelRatioHeaderValue()) {
    UseCounter::Count(GetDocument(), WebFeature::kClientHintsContentDPR);
    image_device_pixel_ratio_ =
        1 / image_resource_->CachedImage()->DevicePixelRatioHeaderValue();
  }

  // The replaced content transform depends on the intrinsic size (see:
  // FragmentPaintPropertyTreeBuilder::UpdateReplacedContentTransform).
  SetNeedsPaintPropertyUpdate();

  if (!UpdateNaturalSizeIfNeeded() || !InvalidateLayoutOnNaturalSizeChange()) {
    InvalidatePaintWithoutLayoutChange(defer);
  }

  if (!did_increment_visually_non_empty_pixel_count_) {
    PhysicalSize default_object_size{LayoutUnit(kDefaultWidth),
                                     LayoutUnit(kDefaultHeight)};
    default_object_size.Scale(StyleRef().EffectiveZoom());
    PhysicalSize concrete_object_size =
        ConcreteObjectSize(natural_dimensions_, default_object_size);
    concrete_object_size.Scale(1 / StyleRef().EffectiveZoom());
    View()->GetFrameView()->IncrementVisuallyNonEmptyPixelCount(
        ToFlooredSize(concrete_object_size));
    did_increment_visually_non_empty_pixel_count_ = true;
  }
}

namespace {

bool CanQueryNaturalSize(const LayoutImageResource& image_resource) {
  if (RuntimeEnabledFeatures::
          LayoutImageEmptyNaturalSizeBeforeSizeAvailableEnabled()) {
    return image_resource.IsSizeAvailable();
  }
  return image_resource.HasImage();
}

}  // namespace

bool LayoutImage::UpdateNaturalSizeIfNeeded() {
  NOT_DESTROYED();
  PhysicalNaturalSizingInfo new_natural_dimensions;
  // If the image resource is not associated with an image then we set natural
  // dimensions of 0x0 ("represents nothing" per HTML spec).
  if (CanQueryNaturalSize(*image_resource_)) {
    new_natural_dimensions = PhysicalNaturalSizingInfo::FromSizingInfo(
        image_resource_->GetNaturalDimensions(StyleRef().EffectiveZoom()));
  }
  const bool dimensions_changed = natural_dimensions_ != new_natural_dimensions;
  if (!image_resource_->ErrorOccurred()) {
    natural_dimensions_ = new_natural_dimensions;
  }
  return dimensions_changed;
}

bool LayoutImage::NeedsLayoutOnNaturalSizeChange() const {
  NOT_DESTROYED();
  // Flex layout algorithm uses the intrinsic image width/height even if
  // width/height are specified.
  if (IsFlexItem()) {
    return true;
  }

  const auto& style = StyleRef();
  // TODO(https://crbug.com/313072): Should this test min/max-height as well?
  bool is_fixed_sized =
      style.LogicalWidth().IsFixed() && style.LogicalHeight().IsFixed() &&
      (style.LogicalMinWidth().IsFixed() || style.LogicalMinWidth().IsAuto()) &&
      (style.LogicalMaxWidth().IsFixed() || style.LogicalMaxWidth().IsNone());
  return !is_fixed_sized;
}

ResourcePriority LayoutImage::ComputeResourcePriority() const {
  speculative_decode_parameters_.cached_resource_priority.emplace(
      LayoutReplaced::ComputeResourcePriority());
  return speculative_decode_parameters_.cached_resource_priority.value();
}

std::optional<ResourcePriority> LayoutImage::CachedResourcePriority() const {
  return speculative_decode_parameters_.cached_resource_priority;
}

gfx::Size LayoutImage::ComputeSpeculativeDecodeSize() const {
  speculative_decode_parameters_.cached_speculative_decode_size =
      LayoutReplaced::ComputeSpeculativeDecodeSize();
  return speculative_decode_parameters_.cached_speculative_decode_size;
}

gfx::Size LayoutImage::CachedSpeculativeDecodeSize() const {
  return speculative_decode_parameters_.cached_speculative_decode_size;
}

InterpolationQuality LayoutImage::ComputeSpeculativeDecodeQuality() const {
  speculative_decode_parameters_.cached_speculative_decode_quality =
      LayoutReplaced::ComputeSpeculativeDecodeQuality();
  return speculative_decode_parameters_.cached_speculative_decode_quality;
}

InterpolationQuality LayoutImage::CachedSpeculativeDecodeQuality() const {
  return speculative_decode_parameters_.cached_speculative_decode_quality;
}

bool LayoutImage::InvalidateLayoutOnNaturalSizeChange() {
  SetIntrinsicLogicalWidthsDirty();

  if (!NeedsLayoutOnNaturalSizeChange()) {
    return false;
  }
  SetNeedsLayoutAndFullPaintInvalidation(
      layout_invalidation_reason::kSizeChanged);
  return true;
}

void LayoutImage::InvalidatePaintWithoutLayoutChange(
    CanDeferInvalidation defer) {
  NOT_DESTROYED();
  SetShouldDoFullPaintInvalidationWithoutLayoutChange(
      PaintInvalidationReason::kImage);

  if (defer == CanDeferInvalidation::kYes && ImageResource() &&
      ImageResource()->MaybeAnimated())
    SetShouldDelayFullPaintInvalidation();
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

  if (image_resource_ && image_resource_->MaybeAnimated()) {
    if (const auto* cached_image = image_resource_->CachedImage();
        cached_image && (cached_image->NumberOfObservers() > 2)) {
      // Images have 2 observers HTMLImageLoader and LayoutImage, when they're
      // repeated in the same document they'll have more than 2.
      UseCounter::Count(GetDocument(),
                        WebFeature::kAnimatedImageUsedMoreThanOnce);
    }
  }
}

void LayoutImage::AreaElementFocusChanged(HTMLAreaElement* area_element) {
  NOT_DESTROYED();
  DCHECK_EQ(area_element->ImageElement(), GetNode());

  if (area_element->GetPath(this).IsEmpty())
    return;

  InvalidatePaintWithoutLayoutChange(CanDeferInvalidation::kYes);
}

bool LayoutImage::ForegroundIsKnownToBeOpaqueInRect(
    const PhysicalRect& local_rect,
    unsigned) const {
  NOT_DESTROYED();
  if (ChildPaintBlockedByDisplayLock())
    return false;
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
  // Object-view-box may leave parts of the content box empty.
  if (StyleRef().ObjectViewBox()) {
    return false;
  }
  // Check for image with alpha.
  DEVTOOLS_TIMELINE_TRACE_EVENT_WITH_CATEGORIES(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
      inspector_paint_image_event::Data, this, *image_content);
  return image_content->GetImage()->IsOpaque();
}

bool LayoutImage::ComputeBackgroundIsKnownToBeObscured() const {
  NOT_DESTROYED();
  if (!StyleRef().HasBackground())
    return false;

  return ForegroundIsKnownToBeOpaqueInRect(BackgroundPaintedExtent(), 0);
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
                              HitTestPhase phase) {
  NOT_DESTROYED();
  HitTestResult temp_result(result);
  bool inside = LayoutReplaced::NodeAtPoint(temp_result, hit_test_location,
                                            accumulated_offset, phase);

  if (!inside && result.GetHitTestRequest().ListBased())
    result.Append(temp_result);
  if (inside)
    result = temp_result;
  return inside;
}

PhysicalNaturalSizingInfo LayoutImage::GetNaturalDimensions() const {
  NOT_DESTROYED();
  return natural_dimensions_;
}

bool LayoutImage::IsUnsizedImage() const {
  NOT_DESTROYED();
  const ComputedStyle& style = this->StyleRef();
  const auto explicit_width = style.LogicalWidth().HasOnlyFixedAndPercent();
  const auto explicit_height = style.LogicalHeight().HasOnlyFixedAndPercent();
  bool has_aspect_ratio =
      style.AspectRatio().GetType() == EAspectRatioType::kRatio;
  const bool is_fixed_size =
      (explicit_width && explicit_height) ||
      (has_aspect_ratio && (explicit_width || explicit_height));
  return !is_fixed_size;
}

void LayoutImage::MutableForPainting::UpdatePaintedRect(
    const PhysicalRect& paint_rect) {
  // As an optimization for sprite sheets, an image may use the cull rect when
  // generating the display item. We need to invalidate the display item if
  // this rect changes.
  auto& image = To<LayoutImage>(layout_object_);
  if (image.last_paint_rect_ != paint_rect) {
    static_cast<const DisplayItemClient&>(layout_object_).Invalidate();
  }

  image.last_paint_rect_ = paint_rect;
}

}  // namespace blink
