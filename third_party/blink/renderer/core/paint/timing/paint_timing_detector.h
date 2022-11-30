// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_DETECTOR_H_

#include <queue>

#include "base/auto_reset.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_visualizer.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class Image;
class ImagePaintTimingDetector;
class ImageRecord;
class ImageResourceContent;
class LargestContentfulPaintCalculator;
class LayoutObject;
class LocalFrameView;
class PropertyTreeStateOrAlias;
class MediaTiming;
class StyleFetchedImage;
class TextPaintTimingDetector;

// |PaintTimingCallbackManager| is an interface between
// |ImagePaintTimingDetector|/|TextPaintTimingDetector| and |ChromeClient|.
// As |ChromeClient| is shared among the paint-timing-detecters, it
// makes it hard to test each detector without being affected other detectors.
// The interface, however, allows unit tests to mock |ChromeClient| for each
// detector. With the mock, |ImagePaintTimingDetector|'s callback does not need
// to store in the same queue as |TextPaintTimingDetector|'s. The separate
// queue makes it possible to pop an |ImagePaintTimingDetector|'s callback
// without having to popping the |TextPaintTimingDetector|'s.
class PaintTimingCallbackManager : public GarbageCollectedMixin {
 public:
  using LocalThreadCallback = base::OnceCallback<void(base::TimeTicks)>;
  using CallbackQueue = std::queue<LocalThreadCallback>;

  virtual void RegisterCallback(
      PaintTimingCallbackManager::LocalThreadCallback) = 0;
};

// This class is responsible for managing the swap-time callback for Largest
// Image Paint and Largest Text Paint. In frames where both text and image are
// painted, Largest Image Paint and Largest Text Paint need to assign the same
// paint-time for their records. In this case, |PaintTimeCallbackManager|
// requests a swap-time callback and share the swap-time with LIP and LTP.
// Otherwise LIP and LTP would have to request their own swap-time callbacks.
// An extra benefit of this design is that |LargestContentfulPaintCalculator|
// can thus hook to the end of the LIP and LTP's record assignments.
//
// |GarbageCollected| inheritance is required by the swap-time callback
// registration.
class CORE_EXPORT PaintTimingCallbackManagerImpl final
    : public GarbageCollected<PaintTimingCallbackManagerImpl>,
      public PaintTimingCallbackManager {
 public:
  PaintTimingCallbackManagerImpl(LocalFrameView* frame_view)
      : frame_view_(frame_view),
        frame_callbacks_(
            std::make_unique<std::queue<
                PaintTimingCallbackManager::LocalThreadCallback>>()) {}
  ~PaintTimingCallbackManagerImpl() { frame_callbacks_.reset(); }

  // Instead of registering the callback right away, this impl of the interface
  // combine the callback into |frame_callbacks_| before registering a separate
  // swap-time callback for the combined callbacks. When the swap-time callback
  // is invoked, the swap-time is then assigned to each callback of
  // |frame_callbacks_|.
  void RegisterCallback(
      PaintTimingCallbackManager::LocalThreadCallback callback) override {
    frame_callbacks_->push(std::move(callback));
  }

  void RegisterPaintTimeCallbackForCombinedCallbacks();

  inline size_t CountCallbacks() { return frame_callbacks_->size(); }

  void ReportPaintTime(
      std::unique_ptr<std::queue<
          PaintTimingCallbackManager::LocalThreadCallback>> frame_callbacks,
      base::TimeTicks paint_time);

  void Trace(Visitor* visitor) const override;

 private:
  Member<LocalFrameView> frame_view_;
  // |frame_callbacks_| stores the callbacks of |TextPaintTimingDetector| and
  // |ImagePaintTimingDetector| in an (animated) frame. It is passed as an
  // argument of a swap-time callback which once is invoked, invokes every
  // callback in |frame_callbacks_|. This hierarchical callback design is to
  // reduce the need of calling ChromeClient to register swap-time callbacks for
  // both detectos.
  // Although |frame_callbacks_| intends to store callbacks
  // of a frame, it occasionally has to do that for more than one frame, when it
  // fails to register a swap-time callback.
  std::unique_ptr<PaintTimingCallbackManager::CallbackQueue> frame_callbacks_;
};

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
    : public GarbageCollected<PaintTimingDetector> {
  friend class ImagePaintTimingDetectorTest;
  friend class TextPaintTimingDetectorTest;

 public:
  PaintTimingDetector(LocalFrameView*);

  struct LargestContentfulPaintDetails {
    base::TimeTicks largest_image_paint_time_;
    uint64_t largest_image_paint_size_ = 0;
    blink::LargestContentfulPaintType largest_contentful_paint_type_ =
        blink::LargestContentfulPaintType::kNone;
    double largest_contentful_paint_image_bpp_ = 0.0;
    base::TimeTicks largest_text_paint_time_;
    uint64_t largest_text_paint_size_ = 0;
    base::TimeTicks largest_contentful_paint_time_;
    absl::optional<WebURLRequest::Priority>
        largest_contentful_paint_image_request_priority_;
  };

  // Returns true if the image might ultimately be a candidate for largest
  // paint, otherwise false. When this method is called we do not know the
  // largest status for certain, because we need to wait for presentation.
  // Hence the "maybe" return value.
  static bool NotifyBackgroundImagePaint(
      const Node&,
      const Image&,
      const StyleFetchedImage&,
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
  inline static void NotifyTextPaint(const gfx::Rect& text_visual_rect);

  void NotifyImageFinished(const LayoutObject&, const MediaTiming*);
  void LayoutObjectWillBeDestroyed(const LayoutObject&);
  void NotifyImageRemoved(const LayoutObject&, const ImageResourceContent*);
  void NotifyPaintFinished();
  void NotifyInputEvent(WebInputEvent::Type);
  bool NeedToNotifyInputOrScroll() const;
  void NotifyScroll(mojom::blink::ScrollType);

  // The returned value indicates whether the candidates have changed.
  bool NotifyMetricsIfLargestImagePaintChanged(
      base::TimeTicks image_paint_time,
      uint64_t image_size,
      ImageRecord* image_record,
      double image_bpp,
      absl::optional<WebURLRequest::Priority> priority);
  bool NotifyMetricsIfLargestTextPaintChanged(base::TimeTicks, uint64_t size);

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
  void StartRecordingLCP();

  LargestContentfulPaintCalculator* GetLargestContentfulPaintCalculator();

  base::TimeTicks LargestImagePaintForMetrics() const {
    return lcp_details_for_ukm_.largest_image_paint_time_;
  }
  uint64_t LargestImagePaintSizeForMetrics() const {
    return lcp_details_for_ukm_.largest_image_paint_size_;
  }
  blink::LargestContentfulPaintType LargestContentfulPaintTypeForMetrics()
      const {
    return lcp_details_for_ukm_.largest_contentful_paint_type_;
  }
  double LargestContentfulPaintImageBPPForMetrics() const {
    return lcp_details_for_ukm_.largest_contentful_paint_image_bpp_;
  }
  base::TimeTicks LargestTextPaintForMetrics() const {
    return lcp_details_for_ukm_.largest_text_paint_time_;
  }
  uint64_t LargestTextPaintSizeForMetrics() const {
    return lcp_details_for_ukm_.largest_text_paint_size_;
  }

  absl::optional<WebURLRequest::Priority>
  LargestContentfulPaintImageRequestPriorityForMetrics() const {
    return lcp_details_for_ukm_
        .largest_contentful_paint_image_request_priority_;
  }

  base::TimeTicks LargestContentfulPaintForMetrics() const {
    return lcp_details_for_ukm_.largest_contentful_paint_time_;
  }

  base::TimeTicks FirstInputOrScrollNotifiedTimestamp() const {
    return first_input_or_scroll_notified_timestamp_;
  }

  void UpdateLargestContentfulPaintCandidate();

  // Reports the largest image and text candidates painted under non-nested 0
  // opacity layer.
  void ReportIgnoredContent();

  absl::optional<PaintTimingVisualizer>& Visualizer() { return visualizer_; }
  void Trace(Visitor* visitor) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ImagePaintTimingDetectorTest,
                           LargestImagePaint_Detached_Frame);

  // Method called to stop recording the Largest Contentful Paint.
  void OnInputOrScroll();
  bool HasLargestImagePaintChanged(base::TimeTicks, uint64_t size) const;
  bool HasLargestTextPaintChanged(base::TimeTicks, uint64_t size) const;
  void UpdateLargestContentfulPaintTime();
  Member<LocalFrameView> frame_view_;
  // This member lives forever because it is also used for Text Element Timing.
  Member<TextPaintTimingDetector> text_paint_timing_detector_;
  // This member lives forever, to detect LCP entries for soft navigations.
  Member<ImagePaintTimingDetector> image_paint_timing_detector_;

  // This member lives for as long as the largest contentful paint is being
  // computed. However, it is initialized lazily, so it may be nullptr because
  // it has not yet been initialized or because we have stopped computing LCP.
  Member<LargestContentfulPaintCalculator> largest_contentful_paint_calculator_;
  // Time at which the first input or scroll is notified to PaintTimingDetector,
  // hence causing LCP to stop being recorded. This is the same time at which
  // |largest_contentful_paint_calculator_| is set to nullptr.
  base::TimeTicks first_input_or_scroll_notified_timestamp_;

  Member<PaintTimingCallbackManagerImpl> callback_manager_;

  absl::optional<PaintTimingVisualizer> visualizer_;

  LargestContentfulPaintDetails lcp_details_;
  LargestContentfulPaintDetails lcp_details_for_ukm_;
  bool record_lcp_to_ukm_ = true;
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
    // Ideally we'd assert that |top_| exists, but there may be text nodes that
    // do not have an ancestor non-anonymous block layout objects in the layout
    // tree. An example of this is a multicol div, since the
    // LayoutMultiColumnFlowThread is in a different layer from the DIV. In
    // these cases, |top_| will be null. This is a known bug, see the related
    // crbug.com/933479.
    if (top_ && top_->data_)
      top_->data_->aggregated_visual_rect_.Union(visual_rect);
  }

  absl::optional<base::AutoReset<ScopedPaintTimingDetectorBlockPaintHook*>>
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
  absl::optional<Data> data_;
  static ScopedPaintTimingDetectorBlockPaintHook* top_;
};

// static
inline void PaintTimingDetector::NotifyTextPaint(
    const gfx::Rect& text_visual_rect) {
  if (IgnorePaintTimingScope::ShouldIgnore())
    return;
  ScopedPaintTimingDetectorBlockPaintHook::AggregateTextPaint(text_visual_rect);
}

class LCPRectInfo {
  USING_FAST_MALLOC(LCPRectInfo);

 public:
  LCPRectInfo(const gfx::Rect& frame_rect_info, const gfx::Rect& root_rect_info)
      : frame_rect_info_(frame_rect_info), root_rect_info_(root_rect_info) {}

  void OutputToTraceValue(TracedValue&) const;

 private:
  gfx::Rect frame_rect_info_;
  gfx::Rect root_rect_info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_DETECTOR_H_
