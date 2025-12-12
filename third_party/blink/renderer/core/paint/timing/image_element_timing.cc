// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"

#include <optional>

#include "base/time/time.h"
#include "components/viz/common/frame_timing_details.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/paint/timing/element_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
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
  const auto* element = DynamicTo<Element>(layout_object.GetNode());

  return IsExplicitlyRegisteredForElementTiming(element);
}

bool ContributesToContainerTiming(const Element* element) {
  if (!RuntimeEnabledFeatures::ContainerTimingEnabled()) {
    return false;
  }
  return (element && ContainerTiming::ContributesToContainerTiming(element));
}

bool ContributesToContainerTiming(const LayoutObject& layout_object) {
  const auto* element = DynamicTo<Element>(layout_object.GetNode());
  return ContributesToContainerTiming(element);
}

bool NeededForTiming(const LayoutObject& layout_object) {
  return IsExplicitlyRegisteredForElementTiming(layout_object) ||
         ContributesToContainerTiming(layout_object);
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
  if (!internal::NeededForTiming(layout_object)) {
    return;
  }

  const auto& insertion_result = images_notified_.insert(
      MediaRecordId::GenerateHash(&layout_object, cached_image), ImageInfo());
  if (insertion_result.is_new_entry) {
    insertion_result.stored_value->value.load_time_ = base::TimeTicks::Now();
  }
}

void ImageElementTiming::NotifyBackgroundImageFinished(
    const StyleFetchedImage* style_image) {
  const auto& insertion_result =
      background_image_timestamps_.insert(style_image, base::TimeTicks());
  if (insertion_result.is_new_entry) {
    insertion_result.stored_value->value = base::TimeTicks::Now();
  }
}

base::TimeTicks ImageElementTiming::GetBackgroundImageLoadTime(
    const StyleImage* style_image) {
  const auto it = background_image_timestamps_.find(style_image);
  if (it == background_image_timestamps_.end()) {
    return base::TimeTicks();
  }
  return it->value;
}

void ImageElementTiming::NotifyImagePainted(
    const LayoutObject& layout_object,
    const ImageResourceContent& cached_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  if (!internal::NeededForTiming(layout_object)) {
    return;
  }

  auto it = images_notified_.find(
      MediaRecordId::GenerateHash(&layout_object, &cached_image));
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
    const gfx::Rect& image_border) {
  LocalFrame* frame = GetSupplementable()->GetFrame();
  DCHECK(frame == layout_object.GetDocument().GetFrame());
  // Background images could cause |node| to not be an element. For example,
  // style applied to body causes this node to be a Document Node. Therefore,
  // bail out if that is the case.
  auto* element = DynamicTo<Element>(node);
  if (!frame || !element) {
    return;
  }

  // We do not expose elements in shadow trees, for now. We might expose
  // something once the discussions at
  // https://github.com/WICG/element-timing/issues/3 and
  // https://github.com/w3c/webcomponents/issues/816 have been resolved.
  if (node.IsInShadowTree()) {
    return;
  }

  // Do not expose elements which should have effective zero opacity.
  // We can afford to call this expensive method because this is only called
  // once per image annotated with the elementtiming attribute.
  if (!layout_object.HasNonZeroEffectiveOpacity()) {
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
  ExecutionContext* context = layout_object.GetDocument().GetExecutionContext();
  DCHECK(GetSupplementable()->document() == &layout_object.GetDocument());
  DCHECK(context->GetSecurityOrigin());

  // If the image URL is a data URL ("data:image/..."), then the |name| of the
  // PerformanceElementTiming entry should be the URL trimmed to 100 characters.
  // If it is not, then pass in the full URL regardless of the length to be
  // consistent with Resource Timing.
  const String& image_string = url.GetString();
  const String& image_url = url.ProtocolIsData()
                                ? image_string.Left(kInlineImageMaxChars)
                                : image_string;
  if (!element_timings_) {
    element_timings_ =
        MakeGarbageCollected<GCedHeapVector<Member<ElementTimingInfo>>>();
  }
  element_timings_->emplace_back(MakeGarbageCollected<ElementTimingInfo>(
      image_url, intersection_rect, load_time, attr,
      cached_image.IntrinsicSize(respect_orientation), id, element));
}

OptionalPaintTimingCallback ImageElementTiming::TakePaintTimingCallback() {
  if (!element_timings_) {
    return std::nullopt;
  }

  return BindOnce(
      [](ImageElementTiming* self,
         GCedHeapVector<Member<ElementTimingInfo>>* images,
         const base::TimeTicks&, const DOMPaintTimingInfo& paint_timing_info) {
        if (!self) {
          return;
        }
        WindowPerformance* performance =
            DOMWindowPerformance::performance(*self->GetSupplementable());
        if (!performance) {
          return;
        }
        for (ElementTimingInfo* painted_image : *images) {
          if (internal::IsExplicitlyRegisteredForElementTiming(
                  painted_image->element)) {
            performance->AddElementTiming(
                ImagePaintString(), painted_image->url, painted_image->rect,
                paint_timing_info, painted_image->response_end,
                painted_image->identifier, painted_image->intrinsic_size,
                painted_image->id, painted_image->element);
          }
          if (internal::ContributesToContainerTiming(painted_image->element)) {
            self->EnsureContainerTiming();
            self->container_timing_->OnElementPainted(
                paint_timing_info, painted_image->element, painted_image->rect);
          }
        }
      },
      WrapWeakPersistent(this), WrapPersistent(element_timings_.Release()));
}

void ImageElementTiming::NotifyBackgroundImagePainted(
    Node& node,
    const StyleImage& background_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const gfx::Rect& image_border) {
  const LayoutObject* layout_object = node.GetLayoutObject();
  if (!layout_object) {
    return;
  }

  if (!internal::NeededForTiming(*layout_object)) {
    return;
  }

  const ImageResourceContent* cached_image = background_image.CachedImage();
  if (!cached_image || !cached_image->IsLoaded()) {
    return;
  }

  auto it = background_image_timestamps_.find(&background_image);
  if (it == background_image_timestamps_.end()) {
    // TODO(npm): investigate how this could happen. For now, we set the load
    // time as the current time.
    background_image_timestamps_.insert(&background_image,
                                        base::TimeTicks::Now());
    it = background_image_timestamps_.find(&background_image);
  }

  ImageInfo& info =
      images_notified_
          .insert(MediaRecordId::GenerateHash(layout_object, cached_image),
                  ImageInfo())
          .stored_value->value;
  if (!info.is_painted_) {
    info.is_painted_ = true;
    NotifyImagePaintedInternal(node, *layout_object, *cached_image,
                               current_paint_chunk_properties, it->value,
                               image_border);
  }
}

void ImageElementTiming::NotifyImageRemoved(const LayoutObject* layout_object,
                                            const ImageResourceContent* image) {
  images_notified_.erase(MediaRecordId::GenerateHash(layout_object, image));
}

void ImageElementTiming::EnsureContainerTiming() {
  if (container_timing_) {
    return;
  }
  LocalDOMWindow* window = GetSupplementable();
  DCHECK(window);
  container_timing_ = ContainerTiming::From(*window);
}

void ImageElementTiming::Trace(Visitor* visitor) const {
  visitor->Trace(element_timings_);
  visitor->Trace(background_image_timestamps_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(container_timing_);
}

}  // namespace blink
