// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/element_timing.h"

#include "base/check_deref.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client_impl.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/element_timing_info.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

// static
bool ElementTiming::IsRegisteredForElementTiming(const Element* element) {
  if (!element) {
    return false;
  }

  // If the element has no 'elementtiming' attribute, do not
  // generate timing entries for the element. See
  // https://wicg.github.io/element-timing/#sec-modifications-DOM for report
  // vs. ignore criteria.
  return element->FastHasAttribute(html_names::kElementtimingAttr);
}

// static
bool ElementTiming::IsNeededForElementOrContainerTiming(Node* node) {
  auto* element = DynamicTo<Element>(node);
  // It's possible that the `node` isn't an Element, in which case `element`
  // will be null and we don't report element timing. This happens if the text
  // aggregates all the way up to the LayoutView (in which case `node` is the
  // Document), which happens if the text being painted is for @page margin,
  // since that is on the outside of the DOM.
  if (!element) {
    return false;
  }
  // We do not expose elements in shadow trees, for now. We might expose
  // something once the discussions at
  // https://github.com/WICG/element-timing/issues/3 and
  // https://github.com/w3c/webcomponents/issues/816 have been resolved.
  if (element->IsInShadowTree()) {
    return false;
  }
  return IsRegisteredForElementTiming(element) ||
         ContainerTiming::ContributesToContainerTiming(element);
}

// static
gfx::RectF ElementTiming::ComputeIntersectionRect(
    LocalFrame* frame,
    const gfx::Rect& int_visual_rect,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties) {
  // Compute the visible part of the image/text rect.
  FloatClipRect visual_rect((gfx::RectF(int_visual_rect)));
  GeometryMapper::LocalToAncestorVisualRect(current_paint_chunk_properties,
                                            frame->View()
                                                ->GetLayoutView()
                                                ->FirstFragment()
                                                .LocalBorderBoxProperties(),
                                            visual_rect);
  if (!frame->Client()->IsLocalFrameClientImpl()) {
    return gfx::RectF();
  }
  WebFrameWidgetImpl* widget =
      WebLocalFrameImpl::FromFrame(frame)->LocalRootFrameWidget();
  CHECK(widget);
  return widget->BlinkSpaceToDIPs(visual_rect.Rect());
}

// static
ElementTiming& ElementTiming::From(LocalDOMWindow& window) {
  return CHECK_DEREF(PaintTiming::From(*window.document()).GetElementTiming());
}

ElementTiming::ElementTiming(LocalDOMWindow& window,
                             const ImagePaintTimingDetector& detector)
    : window_(&window),
      performance_(DOMWindowPerformance::performance(window)),
      image_paint_timing_detector_(&detector) {}

bool ElementTiming::CanReportToElementTiming() const {
  CHECK(performance_);
  return performance_->HasObserverFor(PerformanceEntry::kElement) ||
         !performance_->IsElementTimingBufferFull();
}

void ElementTiming::NotifyImagePaint(
    const LayoutObject& layout_object,
    const MediaTiming& media_timing,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  if (paint_timing::ShouldIgnoreImageContentForPaintTiming(layout_object,
                                                           &media_timing)) {
    return;
  }

  // `generating_node` will be null for pseudo-elements in cases where there
  // isn't an associated node, e.g. margin at-rules inside of @page rules (see
  // external/wpt/css/css-page/margin-boxes/content-003-print.html, for
  // example).
  Node* generating_node = layout_object.GeneratingNode();
  if (!generating_node) {
    return;
  }

  Node* node = layout_object.GetNode();
  bool is_image_or_video_element = IsA<HTMLImageElement>(node) ||
                                   IsA<HTMLVideoElement>(node) ||
                                   IsA<SVGImageElement>(node);
  if (!is_image_or_video_element) {
    if (!RuntimeEnabledFeatures::AllImagesPaintedSentToElementTimingEnabled()) {
      return;
    }
    if (IsNeededForElementOrContainerTiming(generating_node)) {
      UseCounter::Count(layout_object.GetDocument(),
                        WebFeature::kImageElementTimingNotImageOrVideoNode);
    }
  }

  NotifyImagePaintedInternal(*generating_node, layout_object, media_timing,
                             current_paint_chunk_properties, image_border,
                             nullptr);
}

void ElementTiming::NotifyBackgroundImagePaint(
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

void ElementTiming::NotifyImagePaintedInternal(
    Node& generating_node,
    const LayoutObject& layout_object,
    const MediaTiming& media_timing,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border,
    const StyleImage* style_image) {
  auto* cached_image = DynamicTo<ImageResourceContent>(media_timing);
  // TODO(crbug.com/537185406): First video frame is not yet supported for
  // element timing. Fix this once `ElementTiming` uses the `ImageRecord`s from
  // ImagePaintTimingDetector.
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
  QueueElementTimingInfoForReportingIfNeeded(
      generating_node, layout_object, *cached_image,
      current_paint_chunk_properties, image_border, load_time);
}

void ElementTiming::QueueElementTimingInfoForReportingIfNeeded(
    Node& generating_node,
    const LayoutObject& layout_object,
    const ImageResourceContent& cached_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border,
    base::TimeTicks load_time) {
  // If this content isn't eligible or needed for element timing or container
  // timing, there's nothing to do. Note that `generating_node` might not be an
  // `Element` for background images, e.g. if a style applied to the body causes
  // this node to be a Document Node. Ignore these images.
  if (!IsNeededForElementOrContainerTiming(&generating_node)) {
    return;
  }

  LocalFrame* frame = window_->GetFrame();
  // This is called during paint, and we should not be painting detached
  // frames.
  CHECK(frame);
  CHECK_EQ(frame, layout_object.GetDocument().GetFrame());

  // Non-elements and shadow tree nodes should have been filtered out by
  // `IsNeededForElementOrContainerTiming()`.
  auto* element = To<Element>(&generating_node);
  CHECK(!generating_node.IsInShadowTree());

  RespectImageOrientationEnum respect_orientation =
      layout_object.StyleRef().ImageOrientation();

  gfx::RectF intersection_rect = ComputeIntersectionRect(
      frame, image_border, current_paint_chunk_properties);

  // If the image URL is a data URL ("data:image/..."), then the |name| of the
  // PerformanceElementTiming entry should be the URL trimmed to 100 characters.
  // If it is not, then pass in the full URL regardless of the length to be
  // consistent with Resource Timing.
  const KURL& url = cached_image.Url();
  const String& image_string = url.GetString();
  const String& image_url = url.ProtocolIsData()
                                ? image_string.substr(0, kInlineImageMaxChars)
                                : image_string;
  DEFINE_STATIC_LOCAL(const AtomicString, kImagePaint, ("image-paint"));
  element_timings_.emplace_back(MakeGarbageCollected<ElementTimingInfo>(
      kImagePaint, image_url, intersection_rect, load_time,
      element->FastGetAttribute(html_names::kElementtimingAttr),
      cached_image.IntrinsicSize(respect_orientation),
      element->GetIdAttribute(), element, performance_->NavigationId()));
}

void ElementTiming::QueueElementTimingInfoForReportingIfNeeded(
    const TextRecord& record) {
  if (!IsNeededForElementOrContainerTiming(record.GetNode())) {
    return;
  }

  // Non-elements and shadow tree nodes should have been filtered out by
  // `IsNeededForElementOrContainerTiming()`.
  auto* element = To<Element>(record.GetNode());
  CHECK(!element->IsInShadowTree());

  DEFINE_STATIC_LOCAL(const AtomicString, kTextPaint, ("text-paint"));
  element_timings_.emplace_back(MakeGarbageCollected<ElementTimingInfo>(
      kTextPaint,
      /*url=*/g_empty_string, record.ElementTimingRect(),
      /*response_end=*/base::TimeTicks(),
      element->FastGetAttribute(html_names::kElementtimingAttr),
      /*intrinsic_size=*/gfx::Size(), element->GetIdAttribute(), element,
      performance_->NavigationId()));
}

HeapVector<Member<ElementTimingInfo>>
ElementTiming::TakeElementTimingsOnPaintFinished() {
  return std::move(element_timings_);
}

PaintTimingClient::Type ElementTiming::GetType() const {
  return Type::kElementTiming;
}

void ElementTiming::OnPaintFinished(
    const HeapVector<Member<ImageRecord>>&,
    const HeapVector<Member<TextRecord>>& text_records) {
  // Ensure image entries queued during paint use the updated navigation ID, if
  // a soft navigation committed in this frame.
  //
  // TODO(crbug.com/535432431): Remove this once image processing uses the list
  // of image records passed here, since PaintTiming guarantees the navigation
  // ID is updated for soft navigations before this runs.
  const PerformanceTimelineEntryIdInfo current_nav_id =
      performance_->NavigationId();
  for (ElementTimingInfo* info : element_timings_) {
    info->navigation_id = current_nav_id;
  }

  for (const auto& record : text_records) {
    if (record->WasPreviouslyReported()) {
      continue;
    }
    QueueElementTimingInfoForReportingIfNeeded(*record);
  }
}

void ElementTiming::OnFramePresented(
    const HeapVector<Member<ImageRecord>>&,
    const HeapVector<Member<TextRecord>>&,
    const HeapVector<Member<ElementTimingInfo>>& element_timings,
    const DOMPaintTimingInfo& paint_timing_info) {
  for (ElementTimingInfo* info : element_timings) {
    OnElementPresented(*info, paint_timing_info);
  }
}

void ElementTiming::OnElementPresented(
    const ElementTimingInfo& element_timing_info,
    const DOMPaintTimingInfo& paint_timing_info) {
  CHECK(performance_);

  if (!element_timing_info.identifier.IsNull() && CanReportToElementTiming()) {
    performance_->AddElementTiming(
        element_timing_info.name, element_timing_info.url,
        element_timing_info.rect, paint_timing_info,
        element_timing_info.response_end, element_timing_info.identifier,
        element_timing_info.intrinsic_size, element_timing_info.id,
        element_timing_info.element, element_timing_info.navigation_id);
  }

  MaybeReportToContainerTimingOnFramePresented(
      paint_timing_info, element_timing_info.element, element_timing_info.rect);
}

void ElementTiming::MaybeReportToContainerTimingOnFramePresented(
    const DOMPaintTimingInfo& paint_timing_info,
    Element* element,
    const gfx::RectF& intersection_rect) {
  if (!EnsureContainerTiming()) {
    return;
  }
  container_timing_->OnElementPainted(paint_timing_info, element,
                                      intersection_rect);
}

void ElementTiming::OnImageRemoved(const LayoutObject& layout_object,
                                   const MediaTiming* media_timing) {
  recorded_images_.erase(
      MediaRecordId::GenerateHash(&layout_object, media_timing));
}

bool ElementTiming::EnsureContainerTiming() {
  if (container_timing_) {
    return true;
  }
  // WindowPerformance memoizes its answer, so it can outlive the live feature
  // state, while ContainerTiming::From() CHECKs the live one. Check it here so
  // a stale cache cannot become a crash.
  if (!RuntimeEnabledFeatures::ContainerTimingEnabled(window_)) {
    return false;
  }
  container_timing_ = ContainerTiming::From(*window_);
  return true;
}

void ElementTiming::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  visitor->Trace(performance_);
  visitor->Trace(container_timing_);
  visitor->Trace(image_paint_timing_detector_);
  visitor->Trace(element_timings_);
}

}  // namespace blink
