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
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_element_timing.h"
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
    const LayoutObject& image_layout_object,
    const StyleImage& background_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  Node* generating_node = image_layout_object.GeneratingNode();
  // `generating_node` can have a null `LayoutObject` if `image_layout_object`
  // is a pseudo-element (e.g. `::before` with a background image) whose
  // originating element has `display: contents`.
  if (!generating_node || !generating_node->GetLayoutObject()) {
    return;
  }
  const ImageResourceContent* cached_image = background_image.CachedImage();
  if (!cached_image) {
    return;
  }
  NotifyImagePaintedInternal(*generating_node, image_layout_object,
                             *cached_image, current_paint_chunk_properties,
                             image_border, &background_image);
}

void ElementTiming::NotifyImagePaintedInternal(
    Node& generating_node,
    const LayoutObject& image_layout_object,
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
      MediaRecordId::GenerateHash(&image_layout_object, cached_image));
  if (!result.is_new_entry) {
    return;
  }

  base::TimeTicks load_time =
      style_image ? image_paint_timing_detector_->LoadTime(*style_image)
                  : image_paint_timing_detector_->LoadTime(&image_layout_object,
                                                           cached_image);
  QueuePerformanceEntriesForReportingIfNeeded(
      generating_node, image_layout_object, *cached_image,
      current_paint_chunk_properties, image_border, load_time);
}

void ElementTiming::QueuePerformanceEntriesForReportingIfNeeded(
    Node& generating_node,
    const LayoutObject& image_layout_object,
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
  CHECK_EQ(frame, image_layout_object.GetDocument().GetFrame());

  // Non-elements and shadow tree nodes should have been filtered out by
  // `IsNeededForElementOrContainerTiming()`.
  auto* element = To<Element>(&generating_node);
  CHECK(!generating_node.IsInShadowTree());

  gfx::RectF intersection_rect = ComputeIntersectionRect(
      frame, image_border, current_paint_chunk_properties);

  if (IsRegisteredForElementTiming(element)) {
    RespectImageOrientationEnum respect_orientation =
        image_layout_object.Style().ImageOrientation();
    gfx::Size intrinsic_size = cached_image.IntrinsicSize(respect_orientation);

    // If the image URL is a data URL ("data:image/..."), then the |name| of the
    // PerformanceElementTiming entry should be the URL trimmed to 100
    // characters. If it is not, then pass in the full URL regardless of the
    // length to be consistent with Resource Timing.
    const KURL& url = cached_image.Url();
    const String& image_string = url.GetString();
    const String& image_url = url.ProtocolIsData()
                                  ? image_string.substr(0, kInlineImageMaxChars)
                                  : image_string;
    DEFINE_STATIC_LOCAL(const AtomicString, kImagePaint, ("image-paint"));
    element_timings_.push_back(PerformanceElementTiming::Create(
        kImagePaint, image_url, intersection_rect,
        performance_->MonotonicTimeToDOMHighResTimeStamp(load_time),
        element->FastGetAttribute(html_names::kElementtimingAttr),
        intrinsic_size.width(), intrinsic_size.height(),
        element->GetIdAttribute(), element, window_));
  }

  if (ContainerTiming::ContributesToContainerTiming(element)) {
    container_timings_.push_back(
        MakeGarbageCollected<ContainerTimingInfo>(intersection_rect, element));
  }
}

void ElementTiming::QueuePerformanceEntriesForReportingIfNeeded(
    const TextRecord& record) {
  if (!IsNeededForElementOrContainerTiming(record.GetNode())) {
    return;
  }

  // Non-elements and shadow tree nodes should have been filtered out by
  // `IsNeededForElementOrContainerTiming()`.
  auto* element = To<Element>(record.GetNode());
  CHECK(!element->IsInShadowTree());

  if (IsRegisteredForElementTiming(element)) {
    DEFINE_STATIC_LOCAL(const AtomicString, kTextPaint, ("text-paint"));
    element_timings_.push_back(PerformanceElementTiming::Create(
        kTextPaint,
        /*url=*/g_empty_string, record.ElementTimingRect(),
        /*load_time=*/0.0,
        element->FastGetAttribute(html_names::kElementtimingAttr),
        /*naturalWidth=*/0, /*naturalHeight=*/0, element->GetIdAttribute(),
        element, window_));
  }

  if (ContainerTiming::ContributesToContainerTiming(element)) {
    container_timings_.push_back(MakeGarbageCollected<ContainerTimingInfo>(
        record.ElementTimingRect(), element));
  }
}

PaintTimingClient::Type ElementTiming::GetType() const {
  return Type::kElementTiming;
}

OptionalPaintTimingCallback ElementTiming::OnPaintFinished(
    const HeapVector<Member<ImageRecord>>&,
    const HeapVector<Member<TextRecord>>& text_records) {
  for (const auto& record : text_records) {
    if (record->WasPreviouslyReported()) {
      continue;
    }
    QueuePerformanceEntriesForReportingIfNeeded(*record);
  }

  if (element_timings_.empty() && container_timings_.empty()) {
    return std::nullopt;
  }

  auto* pending_element_timings =
      MakeGarbageCollected<GCedHeapVector<Member<PerformanceElementTiming>>>(
          std::move(element_timings_));
  auto* pending_container_timings =
      MakeGarbageCollected<GCedHeapVector<Member<ContainerTimingInfo>>>(
          std::move(container_timings_));
  return blink::BindOnce(&ElementTiming::FlushTimingsOnFramePresented,
                         WrapWeakPersistent(this),
                         WrapPersistent(pending_element_timings),
                         WrapPersistent(pending_container_timings));
}

void ElementTiming::FlushTimingsOnFramePresented(
    GCedHeapVector<Member<PerformanceElementTiming>>* element_timings,
    GCedHeapVector<Member<ContainerTimingInfo>>* container_timings,
    const base::TimeTicks&,
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id) {
  CHECK(performance_);
  for (PerformanceElementTiming* entry : *element_timings) {
    entry->FinalizeEntry(paint_timing_info, navigation_id);
    performance_->AddElementTiming(entry);
  }

  if (!container_timings->empty()) {
    EnsureContainerTiming();
    for (ContainerTimingInfo* info : *container_timings) {
      container_timing_->OnElementPainted(paint_timing_info, info->element,
                                          info->intersection_rect);
    }
  }
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
  visitor->Trace(container_timings_);
}

}  // namespace blink
