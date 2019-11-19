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
class Element;
class Page;
class PagePopupChromeClient;
class PagePopupClient;
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

  // Cancel informs the PopupClient that it should initiate shutdown of this
  // popup via ClosePopup(). It is called to indicate the popup was closed due
  // to a user gesture outside the popup or other such reasons, where a default
  // cancelled response can be made.
  //
  // When the user chooses a value in the popup and thus it is closed, or if the
  // origin in the DOM disppears, then the Cancel() step would be skipped and go
  // directly to ClosePopup().
  void Cancel();
  // Once ClosePopup() has been called, the WebPagePopupImpl should be disowned
  // by any clients, and will be reaped when then browser closes its
  // RenderWidget which closes this object. This will call back to the
  // PopupClient to say DidClosePopup(), and to the WebViewImpl to cleanup
  // its reference to the popup.
  //
  // Only HasSamePopupClient() may still be called after ClosePopup() runs.
  void ClosePopup();

  // Returns whether another WebPagePopupImpl has the same PopupClient as this
  // instance. May be called after ClosePopup() has run still, in order to
  // determine if a popup sharing the same client was created immediately after
  // closing one.
  bool HasSamePopupClient(WebPagePopupImpl* other) {
    return other && popup_client_ == other->popup_client_;
  }

  WebWidgetClient* WidgetClient() const { return web_page_popup_client_; }

  LocalDOMWindow* Window();

  // WebWidget implementation.
  WebInputEventResult DispatchBufferedTouchEvents() override;

  // WebPagePopup implementation.
  WebPoint PositionRelativeToOwner() override;
  WebDocument GetDocument() override;
  WebPagePopupClient* GetClientForTesting() const override;

  // PagePopup implementation.
  void PostMessageToPopup(const String& message) override;

  // PageWidgetEventHandler implementation.
  WebInputEventResult HandleKeyEvent(const WebKeyboardEvent&) override;

 private:
  // WebWidget implementation.
  // NOTE: The WebWidget may still be used after requesting the popup to be
  // closed and destroyed. But the Page and the MainFrame are destroyed
  // immediately. So all methods (outside of initialization) that are part
  // of the WebWidget need to check if close has already been initiated (they
  // can do so by checking |page_|) and not crash! https://crbug.com/906340
  void SetAnimationHost(cc::AnimationHost*) override;
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) final;
  void BeginFrame(base::TimeTicks last_frame_time,
                  bool record_main_frame_metrics) override;
  void UpdateLifecycle(LifecycleUpdate requested_update,
                       LifecycleUpdateReason reason) override;
  void Resize(const WebSize&) override;
  void Close() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  void SetFocus(bool) override;
  bool IsAcceleratedCompositingActive() const override {
    return is_accelerated_compositing_active_;
  }
  WebURL GetURLForDebugTrace() override;
  WebHitTestResult HitTestResultAt(const gfx::Point&) override { return {}; }

  // PageWidgetEventHandler functions
  WebInputEventResult HandleCharEvent(const WebKeyboardEvent&) override;
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&) override;
  void HandleMouseDown(LocalFrame& main_frame, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseWheel(LocalFrame& main_frame,
                                       const WebMouseWheelEvent&) override;

  // This may only be called if page_ is non-null.
  LocalFrame& MainFrame() const;

  Element* FocusedElement() const;

  bool IsViewportPointInWindow(int x, int y);

  // PagePopup function
  AXObject* RootAXObject() override;
  void SetWindowRect(const IntRect&) override;

  explicit WebPagePopupImpl(WebPagePopupClient*);
  void DestroyPage();
  void SetRootLayer(scoped_refptr<cc::Layer>);

  WebRect WindowRectInScreen() const;

  WebPagePopupClient* web_page_popup_client_;
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

  cc::AnimationHost* animation_host_ = nullptr;
  scoped_refptr<cc::Layer> root_layer_;
  base::TimeTicks raf_aligned_input_start_time_;
  bool is_accelerated_compositing_active_ = false;

  bool suppress_next_keypress_event_ = false;

  friend class WebPagePopup;
  friend class PagePopupChromeClient;

  DISALLOW_COPY_AND_ASSIGN(WebPagePopupImpl);
};

// WebPagePopupImpl is the only implementation of WebPagePopup and PagePopup, so
// no further checking required.
DEFINE_TYPE_CASTS(WebPagePopupImpl, WebPagePopup, widget, true, true);
DEFINE_TYPE_CASTS(WebPagePopupImpl, PagePopup, popup, true, true);

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_PAGE_POPUP_IMPL_H_
