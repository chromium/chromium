// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing.h"
#include "third_party/blink/renderer/core/paint/timing/media_record_id.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_client.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace blink {

class Element;
class ImagePaintTimingDetector;
class ImageResourceContent;
class LayoutObject;
class MediaTiming;
class Node;
class PropertyTreeStateOrAlias;
class StyleImage;
class TextRecord;
struct ElementTimingInfo;

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
  static constexpr unsigned kInlineImageMaxChars = 100;

  static ElementTiming& From(LocalDOMWindow&);

  // Returns true if the element is marked with the 'elementtiming' attribute
  // and false otherwise.
  static bool IsRegisteredForElementTiming(const Element*);

  // Returns true if the node is eligible (a non-shadow tree element) and needed
  // for element or container timing (either marked with the 'elementtiming'
  // attribute or contributing to container timing), and false otherwise.
  static bool IsNeededForElementOrContainerTiming(Node*);

  // Computes the visible portion of a rect in DIPs relative to the local root.
  static gfx::RectF ComputeIntersectionRect(LocalFrame*,
                                            const gfx::Rect&,
                                            const PropertyTreeStateOrAlias&);

  ElementTiming(LocalDOMWindow&, const ImagePaintTimingDetector&);

  ElementTiming(const ElementTiming&) = delete;
  ElementTiming& operator=(const ElementTiming&) = delete;

  // PaintTimingClient overrides:
  Type GetType() const override;
  void OnPaintFinished(const HeapVector<Member<ImageRecord>>&,
                       const HeapVector<Member<TextRecord>>&) override;
  void OnFramePresented(const HeapVector<Member<ImageRecord>>&,
                        const HeapVector<Member<TextRecord>>&,
                        const HeapVector<Member<ElementTimingInfo>>&,
                        const DOMPaintTimingInfo&) override;
  void OnImageRemoved(const LayoutObject&, const MediaTiming*) override;
  void Trace(Visitor*) const override;

  // Called by `PaintTimingDetector` when an image is painted.
  // `image_layout_object` represents either the node's image (<img> or SVG
  // image) or a pseudo-element's content image.
  //
  // TODO(crbug.com/535432431): Use ImagePaintTimingDetector and remove this.
  void NotifyImagePaint(
      const LayoutObject& image_layout_object,
      const MediaTiming& cached_image,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border);

  // Called by `PaintTimingDetector` when a background image is painted.
  // `image_layout_object` represents either the node's background-image or a
  // pseudo-element's background-image.
  //
  // TODO(crbug.com/535432431): Use ImagePaintTimingDetector and remove this.
  void NotifyBackgroundImagePaint(
      const LayoutObject& image_layout_object,
      const StyleImage& background_image,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border);

  // Takes `ElementTimingInfo`s captured during the current paint phase.
  HeapVector<Member<ElementTimingInfo>> TakeElementTimingsOnPaintFinished();

 private:
  friend class ElementTimingTest;

  bool CanReportToElementTiming() const;

  void OnElementPresented(const ElementTimingInfo&, const DOMPaintTimingInfo&);

  void NotifyImagePaintedInternal(
      Node& generating_node,
      const LayoutObject& image_layout_object,
      const MediaTiming&,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border,
      const StyleImage*);

  void QueueElementTimingInfoForReportingIfNeeded(
      Node& generating_node,
      const LayoutObject& image_layout_object,
      const ImageResourceContent&,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border,
      base::TimeTicks load_time);
  void QueueElementTimingInfoForReportingIfNeeded(const TextRecord&);

  bool EnsureContainerTiming();

  void MaybeReportToContainerTimingOnFramePresented(
      const DOMPaintTimingInfo&,
      Element*,
      const gfx::RectF& intersection_rect);

  Member<LocalDOMWindow> window_;
  Member<WindowPerformance> performance_;
  Member<ContainerTiming> container_timing_;
  Member<const ImagePaintTimingDetector> image_paint_timing_detector_;

  // `ElementTimingInfo`s queued during paint to be processed on frame
  // presentation.
  HeapVector<Member<ElementTimingInfo>> element_timings_;

  // Set of images that have already been considered for Element Timing.
  HashSet<MediaRecordIdHash> recorded_images_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_ELEMENT_TIMING_H_
