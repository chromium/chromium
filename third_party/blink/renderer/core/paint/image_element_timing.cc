// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_element_timing.h"

#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// TODO(npm): decide on a reasonable value for the threshold.
constexpr const float kImageTimingSizeThreshold = 0.15f;

// static
const char ImageElementTiming::kSupplementName[] = "ImageElementTiming";

// static
ImageElementTiming& ImageElementTiming::From(LocalDOMWindow& window) {
  ImageElementTiming* timing =
      Supplement<LocalDOMWindow>::From<ImageElementTiming>(window);
  if (!timing) {
    timing = new ImageElementTiming(window);
    ProvideTo(window, timing);
  }
  return *timing;
}

ImageElementTiming::ImageElementTiming(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {
  DCHECK(RuntimeEnabledFeatures::ElementTimingEnabled());
}

void ImageElementTiming::NotifyImagePainted(const HTMLImageElement* element,
                                            const LayoutImage* layout_image,
                                            const PaintLayer* painting_layer) {
  if (images_notified_.find(layout_image) != images_notified_.end())
    return;

  images_notified_.insert(layout_image);

  LocalFrame* frame = GetSupplementable()->GetFrame();
  DCHECK(frame == layout_image->GetDocument().GetFrame());
  if (!frame)
    return;

  // Skip the computations below if the element is not same origin.
  DCHECK(layout_image->CachedImage());
  const KURL& url = layout_image->CachedImage()->Url();
  DCHECK(GetSupplementable()->document() == &layout_image->GetDocument());
  if (!SecurityOrigin::AreSameSchemeHostPort(layout_image->GetDocument().Url(),
                                             url))
    return;

  // Compute the viewport rect.
  WebLayerTreeView* layerTreeView =
      frame->GetChromeClient().GetWebLayerTreeView(frame);
  if (!layerTreeView)
    return;

  IntRect viewport = frame->View()->LayoutViewport()->VisibleContentRect();

  // Compute the visible part of the image rect.
  LayoutRect image_visual_rect = layout_image->FirstFragment().VisualRect();
  const auto* local_transform = painting_layer->GetLayoutObject()
                                    .FirstFragment()
                                    .LocalBorderBoxProperties()
                                    .Transform();
  const auto* ancestor_transform = painting_layer->GetLayoutObject()
                                       .View()
                                       ->FirstFragment()
                                       .LocalBorderBoxProperties()
                                       .Transform();
  FloatRect new_visual_rect_abs = FloatRect(image_visual_rect);
  GeometryMapper::SourceToDestinationRect(local_transform, ancestor_transform,
                                          new_visual_rect_abs);
  IntRect visible_new_visual_rect = RoundedIntRect(new_visual_rect_abs);
  visible_new_visual_rect.Intersect(viewport);

  const AtomicString attr =
      element->FastGetAttribute(HTMLNames::elementtimingAttr);
  // Do not create an entry if 'elementtiming' is not present or the image is
  // below a certain size threshold.
  if (attr.IsEmpty() &&
      visible_new_visual_rect.Size().Area() <=
          viewport.Size().Area() * kImageTimingSizeThreshold) {
    return;
  }

  // Compute the |name| for the entry. Use the 'elementtiming' attribute. If
  // empty, use the ID. If empty, use 'img'.
  AtomicString name = attr;
  if (name.IsEmpty())
    name = element->FastGetAttribute(HTMLNames::idAttr);
  if (name.IsEmpty())
    name = "img";
  element_timings_.emplace_back(name, visible_new_visual_rect);
  // Only queue a swap promise when |element_timings_| was empty. All of the
  // records in |element_timings_| will be processed when the promise succeeds
  // or fails, and at that time the vector is cleared.
  if (element_timings_.size() == 1) {
    layerTreeView->NotifySwapTime(ConvertToBaseCallback(
        CrossThreadBind(&ImageElementTiming::ReportImagePaintSwapTime,
                        WrapCrossThreadWeakPersistent(this))));
  }
}

void ImageElementTiming::ReportImagePaintSwapTime(WebLayerTreeView::SwapResult,
                                                  base::TimeTicks timestamp) {
  Document* document = GetSupplementable()->document();
  DCHECK(document);
  const SecurityOrigin* current_origin = document->GetSecurityOrigin();
  // It suffices to check the current origin against the parent origin since all
  // origins stored in |element_timings_| have been checked against the current
  // origin.
  while (document &&
         current_origin->IsSameSchemeHostPort(document->GetSecurityOrigin())) {
    DCHECK(document->domWindow());
    WindowPerformance* performance =
        DOMWindowPerformance::performance(*document->domWindow());
    if (performance &&
        performance->HasObserverFor(PerformanceEntry::kElement)) {
      for (const auto& element_timing : element_timings_) {
        performance->AddElementTiming(element_timing.name, element_timing.rect,
                                      timestamp);
      }
    }
    // Provide the entry to the parent documents for as long as the origin check
    // still holds.
    document = document->ParentDocument();
  }

  element_timings_.clear();
}

void ImageElementTiming::NotifyWillBeDestroyed(const LayoutImage* image) {
  images_notified_.erase(image);
}

void ImageElementTiming::Trace(blink::Visitor* visitor) {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
