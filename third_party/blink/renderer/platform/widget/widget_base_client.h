// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_CLIENT_H_

#include <vector>

#include "base/time/time.h"
#include "cc/paint/element_id.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/page/widget.mojom-blink.h"
#include "third_party/blink/public/mojom/widget/screen_orientation.mojom-blink.h"
#include "third_party/blink/public/platform/input/input_handler_proxy.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/web/web_lifecycle_update.h"

namespace cc {
class LayerTreeFrameSink;
struct BeginMainFrameMetrics;
class RenderFrameMetadataObserver;
}  // namespace cc

namespace blink {

class FrameWidget;
class WebGestureEvent;
class WebMouseEvent;

// This class is part of the foundation of all widgets. It provides
// callbacks from the compositing infrastructure that the individual widgets
// will need to implement.
class WidgetBaseClient {
 public:
  // Called to record the time taken to dispatch rAF aligned input.
  virtual void RecordDispatchRafAlignedInputTime(
      base::TimeTicks raf_aligned_input_start_time) {}

  // Called to update the document lifecycle, advance the state of animations
  // and dispatch rAF.
  virtual void BeginMainFrame(base::TimeTicks frame_time) = 0;

  // Called to record the time between when the widget was marked visible
  // until the compositor begain producing a frame.
  virtual void RecordTimeToFirstActivePaint(base::TimeDelta duration) = 0;

  // Requests that the lifecycle of the widget be updated.
  virtual void UpdateLifecycle(WebLifecycleUpdate requested_update,
                               DocumentUpdateReason reason) = 0;

  // TODO(crbug.com/704763): Remove the need for this.
  virtual void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) {}

  // Called when main frame metrics are desired. The local frame's UKM
  // aggregator must be informed that collection is starting for the
  // frame.
  virtual void RecordStartOfFrameMetrics() {}

  // Called when a main frame time metric should be emitted, along with
  // any metrics that depend upon the main frame total time.
  virtual void RecordEndOfFrameMetrics(
      base::TimeTicks frame_begin_time,
      cc::ActiveFrameSequenceTrackers trackers) {}

  // Return metrics information for the stages of BeginMainFrame. This is
  // ultimately implemented by Blink's LocalFrameUKMAggregator. It must be a
  // distinct call from the FrameMetrics above because the BeginMainFrameMetrics
  // for compositor latency must be gathered before the layer tree is
  // committed to the compositor, which is before the call to
  // RecordEndOfFrameMetrics.
  virtual std::unique_ptr<cc::BeginMainFrameMetrics>
  GetBeginMainFrameMetrics() {
    return nullptr;
  }

  // Methods called to mark the beginning and end of the
  // LayerTreeHost::UpdateLayers method. Only called when gathering main frame
  // UMA and UKM. That is, when RecordStartOfFrameMetrics has been called, and
  // before RecordEndOfFrameMetrics has been called.
  virtual void BeginUpdateLayers() {}
  virtual void EndUpdateLayers() {}

  // Methods called to mark the beginning and end of a commit to the impl
  // thread for a frame. Only called when gathering main frame
  // UMA and UKM. That is, when RecordStartOfFrameMetrics has been called, and
  // before RecordEndOfFrameMetrics has been called.
  virtual void BeginCommitCompositorFrame() {}
  virtual void EndCommitCompositorFrame(base::TimeTicks commit_start_time) {}

  // Applies viewport related properties during a commit from the compositor
  // thread.
  virtual void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) {}

  virtual void RecordManipulationTypeCounts(cc::ManipulationInfo info) {}

  virtual void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) {}
  virtual void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) {}

  virtual void DidBeginMainFrame() {}
  virtual void DidCommitAndDrawCompositorFrame() {}

  virtual void DidObserveFirstScrollDelay(
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) {}

  using LayerTreeFrameSinkCallback = base::OnceCallback<void(
      std::unique_ptr<cc::LayerTreeFrameSink>,
      std::unique_ptr<cc::RenderFrameMetadataObserver>)>;

  // Requests a LayerTreeFrameSink to submit CompositorFrames to.
  virtual void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) = 0;

  virtual void WillBeginMainFrame() {}
  virtual void DidCompletePageScaleAnimation() {}

  virtual void SubmitThroughputData(ukm::SourceId source_id,
                                    int aggregated_percent,
                                    int impl_percent,
                                    base::Optional<int> main_percent) {}
  virtual void FocusChangeComplete() {}

  virtual WebInputEventResult DispatchBufferedTouchEvents() = 0;
  virtual WebInputEventResult HandleInputEvent(
      const WebCoalescedInputEvent&) = 0;
  virtual bool SupportsBufferedTouchEvents() = 0;

  virtual void DidHandleKeyEvent() {}
  virtual bool WillHandleGestureEvent(const WebGestureEvent& event) = 0;
  virtual bool WillHandleMouseEvent(const WebMouseEvent& event) = 0;
  virtual void ObserveGestureEventAndResult(
      const WebGestureEvent& gesture_event,
      const gfx::Vector2dF& unused_delta,
      const cc::OverscrollBehavior& overscroll_behavior,
      bool event_processed) = 0;

  virtual WebTextInputType GetTextInputType() {
    return WebTextInputType::kWebTextInputTypeNone;
  }

  // Called to inform the Widget of the mouse cursor's visibility.
  virtual void SetCursorVisibilityState(bool is_visible) {}

  // Mouse capture has been lost.
  virtual void MouseCaptureLost() {}

  // The FrameWidget interface if this is a FrameWidget.
  virtual blink::FrameWidget* FrameWidget() { return nullptr; }

  // Called to inform the Widget that it has gained or lost keyboard focus.
  virtual void FocusChanged(bool) = 0;

  // Call to schedule an animation.
  virtual void ScheduleAnimation() {}

  // TODO(bokan): Temporary to unblock synthetic gesture events running under
  // VR. https://crbug.com/940063
  virtual bool ShouldAckSyntheticInputImmediately() { return false; }

  // Apply the visual properties.
  virtual void UpdateVisualProperties(
      const VisualProperties& visual_properties) = 0;

  // A callback to apply the updated screen rects, return true if it
  // was handled. If not handled WidgetBase will apply the screen
  // rects as the new values.
  virtual bool UpdateScreenRects(const gfx::Rect& widget_screen_rect,
                                 const gfx::Rect& window_screen_rect) {
    return false;
  }

  // Convert screen coordinates to device emulated coordinates (scaled
  // coordinates when devtools is used). This occurs for popups where their
  // window bounds are emulated.
  virtual void ScreenRectToEmulated(gfx::Rect& screen_rect) {}
  virtual void EmulatedToScreenRect(gfx::Rect& screen_rect) {}

  // Signal the orientation has changed.
  virtual void OrientationChanged() {}

  // Return the original (non-emulated) screen info.
  virtual const ScreenInfo& GetOriginalScreenInfo() = 0;

  // Indication that the surface and screen were updated.
  virtual void DidUpdateSurfaceAndScreen(
      const ScreenInfo& previous_original_screen_info) {}

  // Return the viewport visible rect.
  virtual gfx::Rect ViewportVisibleRect() = 0;

  // The screen orientation override.
  virtual base::Optional<mojom::blink::ScreenOrientation>
  ScreenOrientationOverride() {
    return base::nullopt;
  }

  // Return the overridden device scale factor for testing.
  virtual float GetDeviceScaleFactorForTesting() { return 0.f; }

  // Test-specific methods below this point.
  virtual void ScheduleAnimationForWebTests() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_CLIENT_H_
