// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing.h"
#include "third_party/blink/renderer/core/paint/timing/media_record_id.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_callbacks.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_client.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct ElementTimingInfo;
class ImagePaintTimingDetector;
class ImageResourceContent;
class PropertyTreeStateOrAlias;
class StyleImage;

// ElementTiming is responsible for tracking the paint timing and emitting
// performance entries for image and text elements marked with a "elementtiming"
// or "containertiming" attribute.
//
// TODO(crbug.com/559075199): Decouple `ContainerTiming` from this class.
class CORE_EXPORT ElementTiming final : public GarbageCollected<ElementTiming>,
                                        public PaintTimingClient {
 public:
  // The maximum amount of characters included in Element Timing and Largest
  // Contentful Paint for inline images.
  static constexpr const unsigned kInlineImageMaxChars = 100;

  ElementTiming(LocalDOMWindow&, const ImagePaintTimingDetector&);
  ElementTiming(const ElementTiming&) = delete;
  ElementTiming& operator=(const ElementTiming&) = delete;

  static ElementTiming& From(LocalDOMWindow&);

  // Checks whether an element or layout object is explicitly marked with the
  // 'elementtiming' attribute.
  static bool IsExplicitlyRegisteredForElementTiming(const Element*);
  static bool IsExplicitlyRegisteredForElementTiming(const LayoutObject&);

  // PaintTimingClient:
  void OnFramePresented(const HeapVector<Member<ImageRecord>>&,
                        const HeapVector<Member<TextRecord>>&,
                        const HeapVector<Member<ElementTimingInfo>>&,
                        const DOMPaintTimingInfo&) override;
  void Trace(Visitor* visitor) const override;

  // Called when the LayoutObject has been painted. Does nothing if the image is
  // not fully loaded. This method might queue a presentation promise to compute
  // and report paint timestamps.
  void NotifyImagePaint(
      const LayoutObject&,
      const MediaTiming& cached_image,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border);

  void NotifyBackgroundImagePaint(
      Node& generating_node,
      const StyleImage& background_image,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border);

  void NotifyImageRemoved(const LayoutObject&,
                          const ImageResourceContent* image);

  HeapVector<Member<ElementTimingInfo>> TakeElementTimingsOnPaintFinished();

 private:
  friend class ElementTimingTest;

  // Only valid at paint time: the answer comes from the tracker, which the
  // pre-paint walk populates.
  bool ContributesToContainerTiming(Element*);
  bool IsNeededForTiming(const LayoutObject&);

  void EnsureContainerTiming();
  bool IsContainerTimingEnabled();

  void NotifyImagePaintedInternal(
      Node& generating_node,
      const LayoutObject&,
      const MediaTiming&,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border,
      const StyleImage*);

  void QueueElementTimingInfoForReportingIfNeeded(
      Node& generating_node,
      const LayoutObject&,
      const ImageResourceContent&,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border,
      base::TimeTicks load_time);

  // Vector containing the element timing infos that will be reported during the
  // next presentation promise callback.
  HeapVector<Member<ElementTimingInfo>> element_timings_;

  // Set of images that have already been considered for Element Timing.
  HashSet<MediaRecordIdHash> recorded_images_;

  Member<ContainerTiming> container_timing_;
  Member<LocalDOMWindow> window_;
  Member<const ImagePaintTimingDetector> image_paint_timing_detector_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_H_
