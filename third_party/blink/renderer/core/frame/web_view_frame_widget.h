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
  void ResizeVisualViewport(const WebSize&) override;
  void DidEnterFullscreen() override;
  void DidExitFullscreen() override;
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) final;
  void BeginFrame(base::TimeTicks last_frame_time) override;
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) override;
  void UpdateLifecycle(LifecycleUpdate requested_update) override;
  void PaintContent(cc::PaintCanvas*, const WebRect& view_port) override;
  void LayoutAndPaintAsync(base::OnceClosure callback) override;
  void CompositeAndReadbackAsync(
      base::OnceCallback<void(const SkBitmap&)>) override;
  void ThemeChanged() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  WebInputEventResult DispatchBufferedTouchEvents() override;
  void SetCursorVisibilityState(bool is_visible) override;
  void ApplyViewportChanges(const ApplyViewportChangesArgs&) override;
  void RecordWheelAndTouchScrollingCount(bool has_scrolled_by_wheel,
                                         bool has_scrolled_by_touch) override;
  void MouseCaptureLost() override;
  void SetFocus(bool) override;
  bool SelectionBounds(WebRect& anchor, WebRect& focus) const override;
  bool IsAcceleratedCompositingActive() const override;
  bool IsWebView() const override { return false; }
  bool IsPagePopup() const override { return false; }
  void WillCloseLayerTreeView() override;
  SkColor BackgroundColor() const override;
  WebPagePopup* GetPagePopup() const override;
  WebURL GetURLForDebugTrace() override;

  // WebFrameWidget overrides:
  void SetVisibilityState(mojom::PageVisibilityState) override;
  void SetBackgroundColorOverride(SkColor) override;
  void ClearBackgroundColorOverride() override;
  void SetBaseBackgroundColorOverride(SkColor) override;
  void ClearBaseBackgroundColorOverride() override;
  void SetBaseBackgroundColor(SkColor) override;
  WebInputMethodController* GetActiveWebInputMethodController() const override;
  bool ScrollFocusedEditableElementIntoView() override;

  // WebFrameWidgetBase overrides:
  void Initialize() override;
  void SetLayerTreeView(WebLayerTreeView*) override;
  bool ForSubframe() const override { return false; }
  void ScheduleAnimation() override;
  base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
  EnsureCompositorMutatorDispatcher(
      scoped_refptr<base::SingleThreadTaskRunner>*) override;
  void SetRootGraphicsLayer(GraphicsLayer*) override;
  GraphicsLayer* RootGraphicsLayer() const override;
  void SetRootLayer(scoped_refptr<cc::Layer>) override;
  WebLayerTreeView* GetLayerTreeView() const override;
  CompositorAnimationHost* AnimationHost() const override;
  WebHitTestResult HitTestResultAt(const WebPoint&) override;
  HitTestResult CoreHitTestResultAt(const WebPoint&) override;

  void Trace(blink::Visitor*) override;

 private:
  PageWidgetEventHandler* GetPageWidgetEventHandler() override;

  scoped_refptr<WebViewImpl> web_view_;

  SelfKeepAlive<WebViewFrameWidget> self_keep_alive_;

  DISALLOW_COPY_AND_ASSIGN(WebViewFrameWidget);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_VIEW_FRAME_WIDGET_H_
