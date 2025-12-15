// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_DETECTOR_H_

#include "base/auto_reset.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_visualizer.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class Image;
class ImagePaintTimingDetector;
class ImageResourceContent;
class LayoutBoxModelObject;
class LayoutObject;
class LocalFrameView;
class Node;
class PropertyTreeStateOrAlias;
class MediaTiming;
class TextPaintTimingDetector;
class StyleImage;

// PaintTimingDetector receives signals regarding text and image paints and
// orchestrates the functionality of more specific paint detectors
// (ImagePaintTimingDetector and TextPaintTimingDetector), to ensure proper
// registration and emission of LCP entries. The class has a dual role, both
// ensuring the emission of web-exposed LCP entries, as well as sending that
// signal towards browser metrics - UKM, UMA and potentially other forms of
// logging implemented by chrome/.
//
// See also:
// https://bit.ly/lcp-explainer
class CORE_EXPORT PaintTimingDetector
    : public GarbageCollected<PaintTimingDetector>,
      public LargestContentfulPaintCalculator::Delegate {
  friend class ImagePaintTimingDetectorTest;
  friend class TextPaintTimingDetectorTest;

 public:
  PaintTimingDetector(LocalFrameView*);

  // Returns true if the image might ultimately be a candidate for largest
  // paint, otherwise false. When this method is called we do not know the
  // largest status for certain, because we need to wait for presentation.
  // Hence the "maybe" return value.
  static bool NotifyBackgroundImagePaint(
      const Node&,
      const Image&,
      const StyleImage&,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border);
  // Returns true if the image is a candidate for largest paint, otherwise
  // false. See the comment for NotifyBackgroundImagePaint(...).
  static bool NotifyImagePaint(
      const LayoutObject&,
      const gfx::Size& intrinsic_size,
      const MediaTiming& media_timing,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border);
  static void NotifyFirstVideoFrame(
      const LayoutObject&,
      const gfx::Size& intrinsic_size,
      const MediaTiming& media_timing,
      const PropertyTreeStateOrAlias& current_paint_chunk_properties,
      const gfx::Rect& image_border);
  inline static void NotifyTextPaint(const gfx::Rect& text_visual_rect);

  // Called when the "src" attribute changes on a <video> element and the change
  // is attributable to an interaction.
  static void NotifyInteractionTriggeredVideoSrcChange(const LayoutObject&);

  // LargestContentfulPaintCalculator::Delegate:
  void EmitLcpPerformanceEntry(const DOMPaintTimingInfo& paint_timing_info,
                               uint64_t paint_size,
                               base::TimeTicks load_time,
                               const AtomicString& id,
                               const String& url,
                               Element* element) override;
  bool IsHardNavigation() const override { return true; }
  void Trace(Visitor* visitor) const override;

  void NotifyImageFinished(const LayoutObject&, const MediaTiming*);
  void LayoutObjectWillBeDestroyed(const LayoutObject&);
  void NotifyImageRemoved(const LayoutObject&, const ImageResourceContent*);
  void NotifyPaintFinished();
  void NotifyInputEvent(WebInputEvent::Type);
  void NotifyScroll(mojom::blink::ScrollType);

  void DidChangePerformanceTiming();

  inline static bool IsTracing() {
    bool tracing_enabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED("loading", &tracing_enabled);
    return tracing_enabled;
  }

  gfx::RectF BlinkSpaceToDIPs(const gfx::RectF& float_rect) const;
  gfx::RectF CalculateVisualRect(const gfx::Rect& visual_rect,
                                 const PropertyTreeStateOrAlias&) const;

  TextPaintTimingDetector& GetTextPaintTimingDetector() const {
    DCHECK(text_paint_timing_detector_);
    return *text_paint_timing_detector_;
  }
  ImagePaintTimingDetector& GetImagePaintTimingDetector() const {
    DCHECK(image_paint_timing_detector_);
    return *image_paint_timing_detector_;
  }
  LargestContentfulPaintCalculator* GetLargestContentfulPaintCalculator();

  const LargestContentfulPaintDetails& LargestContentfulPaintDetailsForMetrics()
      const {
    return lcp_details_for_metrics_;
  }

  const LargestContentfulPaintDetails& LatestLcpDetailsForTest();

  base::TimeTicks FirstInputOrScrollNotifiedTimestamp() const {
    return first_input_or_scroll_notified_timestamp_;
  }

  void UpdateLcpCandidate();

  // Reports the largest image and text candidates painted under non-nested 0
  // opacity layer.
  void ReportIgnoredContent();

  std::optional<PaintTimingVisualizer>& Visualizer() { return visualizer_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ImagePaintTimingDetectorTest,
                           LargestImagePaint_Detached_Frame);

  // Method called to stop recording the Largest Contentful Paint.
  void OnInputOrScroll();

  void UpdateMetricsLcp();

  LocalDOMWindow* DomWindow() const;

  Member<LocalFrameView> frame_view_;
  // This member lives forever because it is also used for Text Element
  // Timing.
  Member<TextPaintTimingDetector> text_paint_timing_detector_;
  // This member lives forever, to detect LCP entries for soft navigations.
  Member<ImagePaintTimingDetector> image_paint_timing_detector_;

  // This member lives for as long as the largest contentful paint is being
  // computed. However, it is initialized lazily, so it may be nullptr because
  // it has not yet been initialized or because we have stopped computing LCP.
  Member<LargestContentfulPaintCalculator> largest_contentful_paint_calculator_;
  // Time at which the first input or scroll is notified to
  // PaintTimingDetector, hence causing LCP to stop being recorded. This is
  // the same time at which |largest_contentful_paint_calculator_| is set to
  // nullptr.
  base::TimeTicks first_input_or_scroll_notified_timestamp_;

  std::optional<PaintTimingVisualizer> visualizer_;

  // The LCP details reported to metrics (UKM).
  LargestContentfulPaintDetails lcp_details_for_metrics_;
};

// Largest Text Paint and Text Element Timing aggregate text nodes by these
// text nodes' ancestors. In order to tell whether a text node is contained by
// another node efficiently, The aggregation relies on the paint order of the
// rendering tree (https://www.w3.org/TR/CSS21/zindex.html). Because of the
// paint order, we can assume that if a text node T is visited during the visit
// of another node B, then B contains T. This class acts as the hook to certain
// container nodes (block object or inline object) to tell whether a text node
// is their descendant. The hook should be placed right before visiting the
// subtree of an container node, so that the constructor and the destructor can
// tell the start and end of the visit.
// TODO(crbug.com/960946): we should document the text aggregation.
class ScopedPaintTimingDetectorBlockPaintHook {
  STACK_ALLOCATED();

 public:
  // This constructor does nothing by itself. It will only set relevant
  // variables when EmplaceIfNeeded() is called successfully. The lifetime of
  // the object helps keeping the lifetime of |reset_top_| and |data_| to the
  // appropriate scope.
  ScopedPaintTimingDetectorBlockPaintHook() {}
  ScopedPaintTimingDetectorBlockPaintHook(
      const ScopedPaintTimingDetectorBlockPaintHook&) = delete;
  ScopedPaintTimingDetectorBlockPaintHook& operator=(
      const ScopedPaintTimingDetectorBlockPaintHook&) = delete;

  void EmplaceIfNeeded(const LayoutBoxModelObject&,
                       const PropertyTreeStateOrAlias&);
  ~ScopedPaintTimingDetectorBlockPaintHook();

 private:
  friend class PaintTimingDetector;
  inline static void AggregateTextPaint(const gfx::Rect& visual_rect) {
    // TODO(crbug.com/40614549): This check was allegedly needed for legacy
    // multicol, but that implementation is now gone. Turn into DCHECK?
    if (top_ && top_->data_) {
      top_->data_->aggregated_visual_rect_.Union(visual_rect);
    }
  }

  std::optional<base::AutoReset<ScopedPaintTimingDetectorBlockPaintHook*>>
      reset_top_;
  struct Data {
    STACK_ALLOCATED();

   public:
    Data(const LayoutBoxModelObject& aggregator,
         const PropertyTreeStateOrAlias&,
         TextPaintTimingDetector*);

    const LayoutBoxModelObject& aggregator_;
    const PropertyTreeStateOrAlias& property_tree_state_;
    TextPaintTimingDetector* detector_;
    gfx::Rect aggregated_visual_rect_;
  };
  std::optional<Data> data_;
  static ScopedPaintTimingDetectorBlockPaintHook* top_;
};

// static
inline void PaintTimingDetector::NotifyTextPaint(
    const gfx::Rect& text_visual_rect) {
  if (IgnorePaintTimingScope::ShouldIgnore()) {
    return;
  }
  ScopedPaintTimingDetectorBlockPaintHook::AggregateTextPaint(text_visual_rect);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_DETECTOR_H_
