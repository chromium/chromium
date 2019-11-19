// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_VIEW_FRAME_WIDGET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_VIEW_FRAME_WIDGET_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/graphics/apply_viewport_changes.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"

namespace blink {

class WebViewImpl;
class WebWidgetClient;

// Shim class to help normalize the widget interfaces in the Blink public API.
// For OOPI, subframes have WebFrameWidgets for input and rendering.
// Unfortunately, the main frame still uses WebView's WebWidget for input and
// rendering. This results in complex code, since there are two different
// implementations of WebWidget and code needs to have branches to handle both
// cases.
// This class allows a Blink embedder to create a WebFrameWidget that can be
// used for the main frame. Internally, it currently wraps WebView's WebWidget
// and just forwards almost everything to it.
// After the embedder starts using a WebFrameWidget for the main frame,
// WebView will be updated to no longer inherit WebWidget. The eventual goal is
// to unfork the widget code duplicated in WebFrameWidgetImpl and WebViewImpl
// into one class.
// A more detailed writeup of this transition can be read at
// https://goo.gl/7yVrnb.
class CORE_EXPORT WebViewFrameWidget : public WebFrameWidgetBase {
 public:
  explicit WebViewFrameWidget(WebWidgetClient&, WebViewImpl&);
  ~WebViewFrameWidget() override;

  // WebWidget overrides:
  void Close() override;
  WebSize Size() override;
  void Resize(const WebSize&) override;
  void DidEnterFullscreen() override;
  void DidExitFullscreen() override;
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) final;
  void BeginFrame(base::TimeTicks last_frame_time,
                  bool record_main_frame_metrics) override;
  void DidBeginFrame() override;
  void BeginRafAlignedInput() override;
  void EndRafAlignedInput() override;
  void BeginUpdateLayers() override;
  void EndUpdateLayers() override;
  void BeginCommitCompositorFrame() override;
  void EndCommitCompositorFrame() override;
  void RecordStartOfFrameMetrics() override;
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) override;
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;
  void UpdateLifecycle(LifecycleUpdate requested_update,
                       LifecycleUpdateReason reason) override;
  void ThemeChanged() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  WebInputEventResult DispatchBufferedTouchEvents() override;
  void SetCursorVisibilityState(bool is_visible) override;
  void OnFallbackCursorModeToggled(bool is_on) override;
  void ApplyViewportChanges(const ApplyViewportChangesArgs&) override;
  void RecordManipulationTypeCounts(cc::ManipulationInfo info) override;
  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) override;
  void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) override;
  void MouseCaptureLost() override;
  void SetFocus(bool) override;
  bool SelectionBounds(WebRect& anchor, WebRect& focus) const override;
  bool IsAcceleratedCompositingActive() const override;
  WebURL GetURLForDebugTrace() override;

  // WebFrameWidget overrides:
  void DidDetachLocalFrameTree() override;
  WebInputMethodController* GetActiveWebInputMethodController() const override;
  bool ScrollFocusedEditableElementIntoView() override;
  WebHitTestResult HitTestResultAt(const gfx::Point&) override;

  // WebFrameWidgetBase overrides:
  void SetAnimationHost(cc::AnimationHost*) override;
  bool ForSubframe() const override { return false; }
  void SetRootLayer(scoped_refptr<cc::Layer>) override;
  cc::AnimationHost* AnimationHost() const override;
  HitTestResult CoreHitTestResultAt(const gfx::Point&) override;
  void ZoomToFindInPageRect(const WebRect& rect_in_root_frame) override;

  void Trace(blink::Visitor*) override;

 private:
  PageWidgetEventHandler* GetPageWidgetEventHandler() override;

  scoped_refptr<WebViewImpl> web_view_;

  SelfKeepAlive<WebViewFrameWidget> self_keep_alive_;

  DISALLOW_COPY_AND_ASSIGN(WebViewFrameWidget);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_VIEW_FRAME_WIDGET_H_
