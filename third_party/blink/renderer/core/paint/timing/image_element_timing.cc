// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"

#include <optional>

#include "base/check_deref.h"
#include "base/time/time.h"
#include "components/viz/common/frame_timing_details.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/paint/timing/element_timing_info.h"
#include "third_party/blink/renderer/core/paint/timing/element_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace internal {

bool IsExplicitlyRegisteredForElementTiming(const Element* element) {
  if (!element) {
    return false;
  }

  // If the element has no 'elementtiming' attribute, do not
  // generate timing entries for the element. See
  // https://wicg.github.io/element-timing/#sec-modifications-DOM for report
  // vs. ignore criteria.
  return element->FastHasAttribute(html_names::kElementtimingAttr);
}

// "CORE_EXPORT" is needed to make this function visible to tests.
bool CORE_EXPORT
IsExplicitlyRegisteredForElementTiming(const LayoutObject& layout_object) {
  const auto* element = DynamicTo<Element>(layout_object.GeneratingNode());

  return IsExplicitlyRegisteredForElementTiming(element);
}
}  // namespace internal


AtomicString ImagePaintString() {
  DEFINE_STATIC_LOCAL(const AtomicString, kImagePaint, ("image-paint"));
  return kImagePaint;
}

// static
ImageElementTiming& ImageElementTiming::From(LocalDOMWindow& window) {
  return CHECK_DEREF(
      PaintTiming::From(*window.document()).GetImageElementTiming());
}

ImageElementTiming::ImageElementTiming(LocalDOMWindow& window,
                                       const ImagePaintTimingDetector& detector)
    : window_(&window), image_paint_timing_detector_(&detector) {}

void ImageElementTiming::NotifyImagePaint(
    const LayoutObject& layout_object,
    const MediaTiming& media_timing,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  Node* node = layout_object.GetNode();
  bool is_image_or_video_element = IsA<HTMLImageElement>(node) ||
                                   IsA<HTMLVideoElement>(node) ||
                                   IsA<SVGImageElement>(node);
  if (!is_image_or_video_element) {
    if (!RuntimeEnabledFeatures::AllImagesPaintedSentToElementTimingEnabled()) {
      return;
    }
    if (NeededForTiming(layout_object)) {
      UseCounter::Count(layout_object.GetDocument(),
                        WebFeature::kImageElementTimingNotImageOrVideoNode);
    }
  }

  // `generating_node` will be null for pseudo-elements in cases where there
  // isn't an associated node, e.g. margin at-rules inside of @page rules (see
  // external/wpt/css/css-page/margin-boxes/content-003-print.html, for
  // example).
  Node* generating_node = layout_object.GeneratingNode();
  if (!generating_node) {
    return;
  }
  NotifyImagePaintedInternal(*generating_node, layout_object, media_timing,
                             current_paint_chunk_properties, image_border,
                             nullptr);
}

void ImageElementTiming::NotifyBackgroundImagePaint(
    Node& generating_node,
    const StyleImage& background_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  const ImageResourceContent* cached_image = background_image.CachedImage();
  if (!cached_image) {
    return;
  }
  NotifyImagePaintedInternal(generating_node,
                             CHECK_DEREF(generating_node.GetLayoutObject()),
                             *cached_image, current_paint_chunk_properties,
                             image_border, &background_image);
}

void ImageElementTiming::NotifyImagePaintedInternal(
    Node& generating_node,
    const LayoutObject& layout_object,
    const MediaTiming& media_timing,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border,
    const StyleImage* style_image) {
  auto* cached_image = DynamicTo<ImageResourceContent>(media_timing);
  // TODO(crbug.com/537185406): First video frame is not yet supported for
  // Element Timing. Fix this once ImageElementTiming is a PaintTiming client.
  if (!cached_image) {
    return;
  }

  // Paint Timing notifies us of paints before images are fully loaded. Ignore
  // those.
  if (!cached_image->IsLoaded()) {
    return;
  }

  // Do not expose elements which should have effective zero opacity or should
  // be otherwise ignored (e.g. paint preview).
  if (IgnorePaintTimingScope::IgnoreDepth()) {
    return;
  }

  // Since the image is loaded, mark it as recorded now so we don't reconsider
  // it later. If the content has already been recorded, there's nothing to do.
  auto result = recorded_images_.insert(
      MediaRecordId::GenerateHash(&layout_object, cached_image));
  if (!result.is_new_entry) {
    return;
  }

  base::TimeTicks load_time =
      style_image ? image_paint_timing_detector_->LoadTime(*style_image)
                  : image_paint_timing_detector_->LoadTime(&layout_object,
                                                           cached_image);
  QueueElementTimingInfoForReporingIfNeeded(
      generating_node, layout_object, *cached_image,
      current_paint_chunk_properties, image_border, load_time);
}

void ImageElementTiming::QueueElementTimingInfoForReporingIfNeeded(
    Node& generating_node,
    const LayoutObject& layout_object,
    const ImageResourceContent& cached_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border,
    base::TimeTicks load_time) {
  // If this content isn't needed for element timing or container timing,
  // there's nothing to do.
  if (!NeededForTiming(layout_object)) {
    return;
  }

  // `generating_node` might not be an `Element` for background images, e.g.  if
  // a style applied to the body causes this node to be a Document Node. Ignore
  // these images.
  LocalFrame* frame = window_->GetFrame();
  CHECK_EQ(frame, layout_object.GetDocument().GetFrame());
  auto* element = DynamicTo<Element>(generating_node);
  if (!frame || !element) {
    return;
  }

  // We do not expose elements in shadow trees, for now. We might expose
  // something once the discussions at
  // https://github.com/WICG/element-timing/issues/3 and
  // https://github.com/w3c/webcomponents/issues/816 have been resolved.
  if (generating_node.IsInShadowTree()) {
    return;
  }

  RespectImageOrientationEnum respect_orientation =
      layout_object.StyleRef().ImageOrientation();

  gfx::RectF intersection_rect = ElementTimingUtils::ComputeIntersectionRect(
      frame, image_border, current_paint_chunk_properties);
  const AtomicString attr =
      element->FastGetAttribute(html_names::kElementtimingAttr);

  const AtomicString& id = element->GetIdAttribute();

  const KURL& url = cached_image.Url();
  DCHECK(window_->document() == &layout_object.GetDocument());
  DCHECK(window_->GetSecurityOrigin());

  // If the image URL is a data URL ("data:image/..."), then the |name| of the
  // PerformanceElementTiming entry should be the URL trimmed to 100 characters.
  // If it is not, then pass in the full URL regardless of the length to be
  // consistent with Resource Timing.
  const String& image_string = url.GetString();
  const String& image_url = url.ProtocolIsData()
                                ? image_string.substr(0, kInlineImageMaxChars)
                                : image_string;
  element_timings_.emplace_back(MakeGarbageCollected<ElementTimingInfo>(
      image_url, intersection_rect, load_time, attr,
      cached_image.IntrinsicSize(respect_orientation), id, element));
}

HeapVector<Member<ElementTimingInfo>>
ImageElementTiming::TakeElementTimingsOnPaintFinished() {
  return std::move(element_timings_);
}

void ImageElementTiming::OnFramePresented(
    const HeapVector<Member<ImageRecord>>&,
    const HeapVector<Member<TextRecord>>&,
    const HeapVector<Member<ElementTimingInfo>>& element_timings,
    const DOMPaintTimingInfo& paint_timing_info) {
  WindowPerformance* performance = DOMWindowPerformance::performance(*window_);
  // `PaintTiming` guarantees that `performance` is non-null.
  CHECK(performance);

  for (ElementTimingInfo* painted_image : element_timings) {
    if (internal::IsExplicitlyRegisteredForElementTiming(
            painted_image->element)) {
      performance->AddElementTiming(
          ImagePaintString(), painted_image->url, painted_image->rect,
          paint_timing_info, painted_image->response_end,
          painted_image->identifier, painted_image->intrinsic_size,
          painted_image->id, painted_image->element);
    }
    if (ContributesToContainerTiming(painted_image->element)) {
      EnsureContainerTiming();
      container_timing_->OnElementPainted(
          paint_timing_info, painted_image->element, painted_image->rect);
    }
  }
}

void ImageElementTiming::NotifyImageRemoved(const LayoutObject& layout_object,
                                            const ImageResourceContent* image) {
  recorded_images_.erase(MediaRecordId::GenerateHash(&layout_object, image));
}

void ImageElementTiming::EnsureContainerTiming() {
  if (container_timing_) {
    return;
  }
  container_timing_ = ContainerTiming::From(*window_);
}

bool ImageElementTiming::ContributesToContainerTiming(Element* element) {
  return element && IsContainerTimingEnabled() &&
         ContainerTiming::ContributesToContainerTiming(element);
}

bool ImageElementTiming::NeededForTiming(const LayoutObject& layout_object) {
  auto* element = DynamicTo<Element>(layout_object.GeneratingNode());
  return internal::IsExplicitlyRegisteredForElementTiming(element) ||
         ContributesToContainerTiming(element);
}

bool ImageElementTiming::IsContainerTimingEnabled() {
  WindowPerformance* performance = DOMWindowPerformance::performance(*window_);
  return performance ? performance->IsContainerTimingEnabled() : false;
}

void ImageElementTiming::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  visitor->Trace(element_timings_);
  visitor->Trace(container_timing_);
  visitor->Trace(image_paint_timing_detector_);
}

}  // namespace blink
