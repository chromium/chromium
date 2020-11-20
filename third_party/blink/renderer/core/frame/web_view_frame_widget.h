// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_VIEW_FRAME_WIDGET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_VIEW_FRAME_WIDGET_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/types/pass_key.h"
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
      base::PassKey<WebFrameWidget>,
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

 private:
  // PageWidgetEventHandler overrides:
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&) override;

  // This stores the last hidden page popup. If a GestureTap attempts to open
  // the popup that is closed by its previous GestureTapDown, the popup remains
  // closed.
  scoped_refptr<WebPagePopupImpl> last_hidden_page_popup_;

  DISALLOW_COPY_AND_ASSIGN(WebViewFrameWidget);
};

// Convenience type for creation method taken by
// InstallCreateWebViewFrameWidgetHook(). The method signature matches the
// WebViewFrameWidget constructor.
using CreateWebViewFrameWidgetFunction =
    WebViewFrameWidget* (*)(base::PassKey<WebFrameWidget>,
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
