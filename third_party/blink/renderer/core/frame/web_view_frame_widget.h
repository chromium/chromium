// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_VIEW_FRAME_WIDGET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_VIEW_FRAME_WIDGET_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/util/type_safety/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/exported/web_page_popup_impl.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/graphics/apply_viewport_changes.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"

namespace blink {

class WebFrameWidget;
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
  WebViewFrameWidget(
      util::PassKey<WebFrameWidget>,
      WebWidgetClient&,
      WebViewImpl&,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const viz::FrameSinkId& frame_sink_id,
      bool is_for_nested_main_frame,
      bool hidden,
      bool never_composited);
  ~WebViewFrameWidget() override;

  // WebWidget overrides:
  void Close(
      scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner) override;
  gfx::Size Size() override;
  void Resize(const gfx::Size& size_with_dsf) override;
  void MouseCaptureLost() override;

  // WebFrameWidget overrides:
  bool ScrollFocusedEditableElementIntoView() override;

  // WebFrameWidgetBase overrides:
  bool ForSubframe() const override { return false; }
  bool ForTopLevelFrame() const override { return !is_for_nested_main_frame_; }
  void SetAutoResizeMode(bool auto_resize,
                         const gfx::Size& min_size_before_dsf,
                         const gfx::Size& max_size_before_dsf,
                         float device_scale_factor) override;
  void SetPageScaleStateAndLimits(float page_scale_factor,
                                  bool is_pinch_gesture_active,
                                  float minimum,
                                  float maximum) override;
  void ApplyVisualPropertiesSizing(
      const VisualProperties& visual_properties) override;

  // FrameWidget overrides:
  void SetRootLayer(scoped_refptr<cc::Layer>) override;
  bool ShouldHandleImeEvents() override;

  // WidgetBaseClient overrides:
  void FocusChanged(bool enabled) override;

  void SetIsNestedMainFrameWidget(bool is_nested);
  void DidAutoResize(const gfx::Size& size);
  void SetDeviceColorSpaceForTesting(const gfx::ColorSpace& color_space);
  void SetWindowRect(const gfx::Rect& window_rect);
  void SetWindowRectSynchronouslyForTesting(const gfx::Rect& new_window_rect);
  void UseSynchronousResizeModeForTesting(bool enable);

  // Converts from DIPs to Blink coordinate space (ie. Viewport/Physical
  // pixels).
  gfx::Size DIPsToCeiledBlinkSpace(const gfx::Size& size);

 private:
  // PageWidgetEventHandler overrides:
  void HandleMouseLeave(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&) override;

  LocalFrameView* GetLocalFrameViewForAnimationScrolling() override;
  void SetWindowRectSynchronously(const gfx::Rect& new_window_rect);

  scoped_refptr<WebViewImpl> web_view_;

  // This bit is used to tell if this is a nested widget (an "inner web
  // contents") like a <webview> or <portal> widget. If false, the widget is the
  // top level widget.
  bool is_for_nested_main_frame_ = false;

  // In web tests, synchronous resizing mode may be used. Normally each widget's
  // size is controlled by IPC from the browser. In synchronous resize mode the
  // renderer controls the size directly, and IPCs from the browser must be
  // ignored. This was deprecated but then later undeprecated, so it is now
  // called unfortunate instead. See https://crbug.com/309760. When this is
  // enabled the various size properties will be controlled directly when
  // SetWindowRect() is called instead of needing a round trip through the
  // browser.
  // Note that SetWindowRectSynchronouslyForTesting() provides a secondary way
  // to control the size of the FrameWidget independently from the renderer
  // process, without the use of this mode, however it would be overridden by
  // the browser if they disagree.
  bool synchronous_resize_mode_for_testing_ = false;

  // The size of the widget in viewport coordinates. This is slightly different
  // than the WebViewImpl::size_ since isn't set in auto resize mode.
  gfx::Size size_;

  // This stores the last hidden page popup. If a GestureTap attempts to open
  // the popup that is closed by its previous GestureTapDown, the popup remains
  // closed.
  scoped_refptr<WebPagePopupImpl> last_hidden_page_popup_;

  SelfKeepAlive<WebViewFrameWidget> self_keep_alive_;

  DISALLOW_COPY_AND_ASSIGN(WebViewFrameWidget);
};

// Convenience type for creation method taken by
// InstallCreateWebViewFrameWidgetHook(). The method signature matches the
// WebViewFrameWidget constructor.
using CreateWebViewFrameWidgetFunction =
    WebViewFrameWidget* (*)(util::PassKey<WebFrameWidget>,
                            WebWidgetClient&,
                            WebViewImpl&,
                            CrossVariantMojoAssociatedRemote<
                                mojom::blink::FrameWidgetHostInterfaceBase>
                                frame_widget_host,
                            CrossVariantMojoAssociatedReceiver<
                                mojom::blink::FrameWidgetInterfaceBase>
                                frame_widget,
                            CrossVariantMojoAssociatedRemote<
                                mojom::blink::WidgetHostInterfaceBase>
                                widget_host,
                            CrossVariantMojoAssociatedReceiver<
                                mojom::blink::WidgetInterfaceBase> widget,
                            scoped_refptr<base::SingleThreadTaskRunner>
                                task_runner,
                            const viz::FrameSinkId& frame_sink_id,
                            bool is_for_nested_main_frame,
                            bool hidden,
                            bool never_composited);
// Overrides the implementation of WebFrameWidget::CreateForMainFrame() function
// below. Used by tests to override some functionality on WebViewFrameWidget.
void CORE_EXPORT InstallCreateWebViewFrameWidgetHook(
    CreateWebViewFrameWidgetFunction create_widget);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_VIEW_FRAME_WIDGET_H_
