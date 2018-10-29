/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_PAGE_POPUP_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_PAGE_POPUP_IMPL_H_

#include "base/macros.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/core/page/page_widget_delegate.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace cc {
class Layer;
}

namespace blink {

class CompositorAnimationHost;
class Page;
class PagePopupChromeClient;
class PagePopupClient;
class WebLayerTreeView;
class WebViewImpl;
class LocalDOMWindow;

class CORE_EXPORT WebPagePopupImpl final : public WebPagePopup,
                                           public PageWidgetEventHandler,
                                           public PagePopup,
                                           public RefCounted<WebPagePopupImpl> {
  USING_FAST_MALLOC(WebPagePopupImpl);

 public:
  ~WebPagePopupImpl() override;
  void Initialize(WebViewImpl*, PagePopupClient*);
  void ClosePopup();
  WebWidgetClient* WidgetClient() const { return widget_client_; }
  bool HasSamePopupClient(WebPagePopupImpl* other) {
    return other && popup_client_ == other->popup_client_;
  }
  LocalDOMWindow* Window();
  void LayoutAndPaintAsync(base::OnceClosure callback) override;
  void CompositeAndReadbackAsync(
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  WebPoint PositionRelativeToOwner() override;
  void PostMessageToPopup(const String& message) override;
  void Cancel();

  // PageWidgetEventHandler functions.
  WebInputEventResult HandleKeyEvent(const WebKeyboardEvent&) override;

  WebInputEventResult DispatchBufferedTouchEvents() override;

 private:
  // WebWidget functions
  void SetLayerTreeView(WebLayerTreeView*) override;
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) final;
  void BeginFrame(base::TimeTicks last_frame_time) override;
  void UpdateLifecycle(LifecycleUpdate requested_update) override;
  void UpdateAllLifecyclePhasesAndCompositeForTesting(bool do_raster) override;
  void WillCloseLayerTreeView() override;
  void PaintContent(cc::PaintCanvas*, const WebRect&) override;
  void Resize(const WebSize&) override;
  void Close() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  void SetFocus(bool) override;
  bool IsPagePopup() const override { return true; }
  bool IsAcceleratedCompositingActive() const override {
    return is_accelerated_compositing_active_;
  }
  WebURL GetURLForDebugTrace() override;

  // PageWidgetEventHandler functions
  WebInputEventResult HandleCharEvent(const WebKeyboardEvent&) override;
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&) override;
  void HandleMouseDown(LocalFrame& main_frame, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseWheel(LocalFrame& main_frame,
                                       const WebMouseWheelEvent&) override;

  // This may only be called if page_ is non-null.
  LocalFrame& MainFrame() const;

  bool IsViewportPointInWindow(int x, int y);

  // PagePopup function
  AXObject* RootAXObject() override;
  void SetWindowRect(const IntRect&) override;

  explicit WebPagePopupImpl(WebWidgetClient*);
  void DestroyPage();
  void SetRootLayer(cc::Layer*);

  WebRect WindowRectInScreen() const;

  WebWidgetClient* widget_client_;
  WebViewImpl* web_view_;
  // WebPagePopupImpl wraps its own Page that renders the content in the popup.
  // This member is non-null between the call to Initialize() and the call to
  // ClosePopup(). If page_ is non-null, it is guaranteed to have an attached
  // main LocalFrame with a corresponding non-null LocalFrameView and non-null
  // Document.
  Persistent<Page> page_;
  Persistent<PagePopupChromeClient> chrome_client_;
  PagePopupClient* popup_client_;
  bool closing_ = false;

  WebLayerTreeView* layer_tree_view_ = nullptr;
  scoped_refptr<cc::Layer> root_layer_;
  std::unique_ptr<CompositorAnimationHost> animation_host_;
  bool is_accelerated_compositing_active_ = false;

  friend class WebPagePopup;
  friend class PagePopupChromeClient;

  DISALLOW_COPY_AND_ASSIGN(WebPagePopupImpl);
};

DEFINE_TYPE_CASTS(WebPagePopupImpl,
                  WebWidget,
                  widget,
                  widget->IsPagePopup(),
                  widget.IsPagePopup());
// WebPagePopupImpl is the only implementation of PagePopup, so no
// further checking required.
DEFINE_TYPE_CASTS(WebPagePopupImpl, PagePopup, popup, true, true);

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_PAGE_POPUP_IMPL_H_
