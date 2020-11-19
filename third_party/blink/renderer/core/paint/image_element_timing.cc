// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_element_timing.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/paint/element_timing_utils.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace internal {

// "CORE_EXPORT" is needed to make this function visible to tests.
bool CORE_EXPORT
IsExplicitlyRegisteredForTiming(const LayoutObject& layout_object) {
  const auto* element = DynamicTo<Element>(layout_object.GetNode());
  if (!element)
    return false;

  // If the element has no 'elementtiming' attribute, do not
  // generate timing entries for the element. See
  // https://wicg.github.io/element-timing/#sec-modifications-DOM for report
  // vs. ignore criteria.
  return element->FastHasAttribute(html_names::kElementtimingAttr);
}

}  // namespace internal

// static
const char ImageElementTiming::kSupplementName[] = "ImageElementTiming";

AtomicString ImagePaintString() {
  DEFINE_STATIC_LOCAL(const AtomicString, kImagePaint, ("image-paint"));
  return kImagePaint;
}

// static
ImageElementTiming& ImageElementTiming::From(LocalDOMWindow& window) {
  ImageElementTiming* timing =
      Supplement<LocalDOMWindow>::From<ImageElementTiming>(window);
  if (!timing) {
    timing = MakeGarbageCollected<ImageElementTiming>(window);
    ProvideTo(window, timing);
  }
  return *timing;
}

ImageElementTiming::ImageElementTiming(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void ImageElementTiming::NotifyImageFinished(
    const LayoutObject& layout_object,
    const ImageResourceContent* cached_image) {
  if (!internal::IsExplicitlyRegisteredForTiming(layout_object))
    return;

  const auto& insertion_result = images_notified_.insert(
      std::make_pair(&layout_object, cached_image), ImageInfo());
  if (insertion_result.is_new_entry)
    insertion_result.stored_value->value.load_time_ = base::TimeTicks::Now();
}

void ImageElementTiming::NotifyBackgroundImageFinished(
    const StyleFetchedImage* style_image) {
  const auto& insertion_result =
      background_image_timestamps_.insert(style_image, base::TimeTicks());
  if (insertion_result.is_new_entry)
    insertion_result.stored_value->value = base::TimeTicks::Now();
}

base::TimeTicks ImageElementTiming::GetBackgroundImageLoadTime(
    const StyleFetchedImage* style_image) {
  return background_image_timestamps_.at(style_image);
}

void ImageElementTiming::NotifyImagePainted(
    const LayoutObject& layout_object,
    const ImageResourceContent& cached_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const IntRect& image_border) {
  if (!internal::IsExplicitlyRegisteredForTiming(layout_object))
    return;

  auto it =
      images_notified_.find(std::make_pair(&layout_object, &cached_image));
  // It is possible that the pair is not in |images_notified_|. See
  // https://crbug.com/1027948
  if (it != images_notified_.end() && !it->value.is_painted_) {
    it->value.is_painted_ = true;
    DCHECK(layout_object.GetNode());
    NotifyImagePaintedInternal(*layout_object.GetNode(), layout_object,
                               cached_image, current_paint_chunk_properties,
                               it->value.load_time_, image_border);
  }
}

void ImageElementTiming::NotifyImagePaintedInternal(
    Node& node,
    const LayoutObject& layout_object,
    const ImageResourceContent& cached_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    base::TimeTicks load_time,
    const IntRect& image_border) {
  LocalFrame* frame = GetSupplementable()->GetFrame();
  DCHECK(frame == layout_object.GetDocument().GetFrame());
  // Background images could cause |node| to not be an element. For example,
  // style applied to body causes this node to be a Document Node. Therefore,
  // bail out if that is the case.
  auto* element = DynamicTo<Element>(node);
  if (!frame || !element)
    return;

  // We do not expose elements in shadow trees, for now. We might expose
  // something once the discussions at
  // https://github.com/WICG/element-timing/issues/3 and
  // https://github.com/w3c/webcomponents/issues/816 have been resolved.
  if (node.IsInShadowTree())
    return;

  // Do not expose elements which should have effective zero opacity.
  // We can afford to call this expensive method because this is only called
  // once per image annotated with the elementtiming attribute.
  if (!layout_object.HasNonZeroEffectiveOpacity())
    return;

  RespectImageOrientationEnum respect_orientation =
      LayoutObject::ShouldRespectImageOrientation(&layout_object);

  FloatRect intersection_rect = ElementTimingUtils::ComputeIntersectionRect(
      frame, image_border, current_paint_chunk_properties);
  const AtomicString attr =
      element->FastGetAttribute(html_names::kElementtimingAttr);

  const AtomicString& id = element->GetIdAttribute();

  const KURL& url = cached_image.Url();
  ExecutionContext* context = layout_object.GetDocument().GetExecutionContext();
  DCHECK(GetSupplementable()->document() == &layout_object.GetDocument());
  DCHECK(context->GetSecurityOrigin());
  // It's ok to expose rendering timestamp for data URIs so exclude those from
  // the Timing-Allow-Origin check.
  if (!url.ProtocolIsData()) {
    if (!cached_image.GetResponse().TimingAllowPassed()) {
      WindowPerformance* performance =
          DOMWindowPerformance::performance(*GetSupplementable());
      if (performance) {
        // Create an entry with a |startTime| of 0.
        performance->AddElementTiming(
            ImagePaintString(), url.GetString(), intersection_rect,
            base::TimeTicks(), load_time, attr,
            cached_image.IntrinsicSize(respect_orientation), id, element);
      }
      return;
    }
  }

  // If the image URL is a data URL ("data:image/..."), then the |name| of the
  // PerformanceElementTiming entry should be the URL trimmed to 100 characters.
  // If it is not, then pass in the full URL regardless of the length to be
  // consistent with Resource Timing.
  const String& image_url = url.ProtocolIsData()
                                ? url.GetString().Left(kInlineImageMaxChars)
                                : url.GetString();
  element_timings_.emplace_back(MakeGarbageCollected<ElementTimingInfo>(
      image_url, intersection_rect, load_time, attr,
      cached_image.IntrinsicSize(respect_orientation), id, element));
  // Only queue a swap promise when |element_timings_| was empty. All of the
  // records in |element_timings_| will be processed when the promise succeeds
  // or fails, and at that time the vector is cleared.
  if (element_timings_.size() == 1) {
    frame->GetChromeClient().NotifySwapTime(
        *frame,
        CrossThreadBindOnce(&ImageElementTiming::ReportImagePaintSwapTime,
                            WrapCrossThreadWeakPersistent(this)));
  }
}

void ImageElementTiming::NotifyBackgroundImagePainted(
    Node& node,
    const StyleFetchedImage& background_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const IntRect& image_border) {
  const LayoutObject* layout_object = node.GetLayoutObject();
  if (!layout_object)
    return;

  if (!internal::IsExplicitlyRegisteredForTiming(*layout_object))
    return;

  const ImageResourceContent* cached_image = background_image.CachedImage();
  if (!cached_image || !cached_image->IsLoaded())
    return;

  auto it = background_image_timestamps_.find(&background_image);
  DCHECK(it != background_image_timestamps_.end());

  ImageInfo& info =
      images_notified_
          .insert(std::make_pair(layout_object, cached_image), ImageInfo())
          .stored_value->value;
  if (!info.is_painted_) {
    info.is_painted_ = true;
    NotifyImagePaintedInternal(node, *layout_object, *cached_image,
                               current_paint_chunk_properties, it->value,
                               image_border);
  }
}

void ImageElementTiming::ReportImagePaintSwapTime(WebSwapResult,
                                                  base::TimeTicks timestamp) {
  WindowPerformance* performance =
      DOMWindowPerformance::performance(*GetSupplementable());
  if (performance) {
    for (const auto& element_timing : element_timings_) {
      performance->AddElementTiming(
          ImagePaintString(), element_timing->url, element_timing->rect,
          timestamp, element_timing->response_end, element_timing->identifier,
          element_timing->intrinsic_size, element_timing->id,
          element_timing->element);
    }
  }
  element_timings_.clear();
}

void ImageElementTiming::NotifyImageRemoved(const LayoutObject* layout_object,
                                            const ImageResourceContent* image) {
  images_notified_.erase(std::make_pair(layout_object, image));
}

void ImageElementTiming::Trace(Visitor* visitor) const {
  visitor->Trace(element_timings_);
  visitor->Trace(background_image_timestamps_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
