// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_DETECTOR_H_

#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/paint_timing_visualizer.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class Image;
class ImagePaintTimingDetector;
class ImageResourceContent;
class LargestContentfulPaintCalculator;
class LayoutObject;
class LocalFrameView;
class PropertyTreeState;
class StyleFetchedImage;
class TextPaintTimingDetector;
struct WebFloatRect;

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
class PaintTimingCallbackManagerImpl final
    : public GarbageCollected<PaintTimingCallbackManagerImpl>,
      public PaintTimingCallbackManager {
  USING_GARBAGE_COLLECTED_MIXIN(PaintTimingCallbackManagerImpl);

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
      WebWidgetClient::SwapResult,
      base::TimeTicks paint_time);

  void Trace(Visitor* visitor) override;

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

// PaintTimingDetector contains some of paint metric detectors,
// providing common infrastructure for these detectors.
//
// Users has to enable 'loading' trace category to enable the metrics.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit
class CORE_EXPORT PaintTimingDetector
    : public GarbageCollected<PaintTimingDetector> {
  friend class ImagePaintTimingDetectorTest;
  friend class TextPaintTimingDetectorTest;

 public:
  PaintTimingDetector(LocalFrameView*);

  static void NotifyBackgroundImagePaint(
      const Node*,
      const Image*,
      const StyleFetchedImage*,
      const PropertyTreeState& current_paint_chunk_properties);
  static void NotifyImagePaint(
      const LayoutObject&,
      const IntSize& intrinsic_size,
      const ImageResourceContent* cached_image,
      const PropertyTreeState& current_paint_chunk_properties);
  inline static void NotifyTextPaint(const IntRect& text_visual_rect);

  void NotifyImageFinished(const LayoutObject&, const ImageResourceContent*);
  void LayoutObjectWillBeDestroyed(const LayoutObject&);
  void NotifyImageRemoved(const LayoutObject&, const ImageResourceContent*);
  void NotifyPaintFinished();
  void NotifyInputEvent(WebInputEvent::Type);
  bool NeedToNotifyInputOrScroll() const;
  void NotifyScroll(ScrollType);
  // The returned value indicates whether the candidates have changed.
  bool NotifyIfChangedLargestImagePaint(base::TimeTicks, uint64_t size);
  bool NotifyIfChangedLargestTextPaint(base::TimeTicks, uint64_t size);

  void DidChangePerformanceTiming();

  inline static bool IsTracing() {
    bool tracing_enabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED("loading", &tracing_enabled);
    return tracing_enabled;
  }

  void ConvertViewportToWindow(WebFloatRect* float_rect) const;
  FloatRect CalculateVisualRect(const IntRect& visual_rect,
                                const PropertyTreeState&) const;

  TextPaintTimingDetector* GetTextPaintTimingDetector() const {
    DCHECK(text_paint_timing_detector_);
    return text_paint_timing_detector_;
  }
  ImagePaintTimingDetector* GetImagePaintTimingDetector() const {
    return image_paint_timing_detector_;
  }

  LargestContentfulPaintCalculator* GetLargestContentfulPaintCalculator();

  base::TimeTicks LargestImagePaint() const {
    return largest_image_paint_time_;
  }
  uint64_t LargestImagePaintSize() const { return largest_image_paint_size_; }
  base::TimeTicks LargestTextPaint() const { return largest_text_paint_time_; }
  uint64_t LargestTextPaintSize() const { return largest_text_paint_size_; }

  void UpdateLargestContentfulPaintCandidate();

  base::Optional<PaintTimingVisualizer>& Visualizer() { return visualizer_; }
  void Trace(Visitor* visitor);

 private:
  void StopRecordingLargestContentfulPaint();
  bool HasLargestImagePaintChanged(base::TimeTicks, uint64_t size) const;
  bool HasLargestTextPaintChanged(base::TimeTicks, uint64_t size) const;
  Member<LocalFrameView> frame_view_;
  // This member lives forever because it is also used for Text Element Timing.
  Member<TextPaintTimingDetector> text_paint_timing_detector_;
  // This member lives until the end of the paint phase after the largest
  // image paint is found.
  Member<ImagePaintTimingDetector> image_paint_timing_detector_;

  Member<LargestContentfulPaintCalculator> largest_contentful_paint_calculator_;

  Member<PaintTimingCallbackManagerImpl> callback_manager_;

  base::Optional<PaintTimingVisualizer> visualizer_;

  // Largest image information.
  base::TimeTicks largest_image_paint_time_;
  uint64_t largest_image_paint_size_ = 0;
  // Largest text information.
  base::TimeTicks largest_text_paint_time_;
  uint64_t largest_text_paint_size_ = 0;
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

  void EmplaceIfNeeded(const LayoutBoxModelObject&, const PropertyTreeState&);
  ~ScopedPaintTimingDetectorBlockPaintHook();

 private:
  friend class PaintTimingDetector;
  inline static void AggregateTextPaint(const IntRect& visual_rect) {
    // Ideally we'd assert that |top_| exists, but there may be text nodes that
    // do not have an ancestor non-anonymous block layout objects in the layout
    // tree. An example of this is a multicol div, since the
    // LayoutMultiColumnFlowThread is in a different layer from the DIV. In
    // these cases, |top_| will be null. This is a known bug, see the related
    // crbug.com/933479.
    if (top_ && top_->data_)
      top_->data_->aggregated_visual_rect_.Unite(visual_rect);
  }

  base::Optional<base::AutoReset<ScopedPaintTimingDetectorBlockPaintHook*>>
      reset_top_;
  struct Data {
    STACK_ALLOCATED();

   public:
    Data(const LayoutBoxModelObject& aggregator,
         const PropertyTreeState&,
         TextPaintTimingDetector*);

    const LayoutBoxModelObject& aggregator_;
    const PropertyTreeState& property_tree_state_;
    Member<TextPaintTimingDetector> detector_;
    IntRect aggregated_visual_rect_;
  };
  base::Optional<Data> data_;
  static ScopedPaintTimingDetectorBlockPaintHook* top_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPaintTimingDetectorBlockPaintHook);
};

// static
inline void PaintTimingDetector::NotifyTextPaint(
    const IntRect& text_visual_rect) {
  ScopedPaintTimingDetectorBlockPaintHook::AggregateTextPaint(text_visual_rect);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_DETECTOR_H_
