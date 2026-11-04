// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_POPUP_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_POPUP_TEST_UTILS_H_

#include <cstdint>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "ipc/constants.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "third_party/blink/public/mojom/page/widget.mojom-test-utils.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include <vector>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom-forward.h"
#endif

// Test helpers that intercept popup widget creation and display. They live
// apart from browser_test_utils.h so that its many includers don't have to
// compile frame.mojom-test-utils.h.

namespace content {

class RenderFrameHost;
class RenderWidgetHost;
class WebContents;

// One-shot helper that listens for creation of a new popup widget.
class CreateNewPopupWidgetInterceptor
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
 public:
  explicit CreateNewPopupWidgetInterceptor(
      RenderFrameHost* rfh,
      base::OnceCallback<void(RenderWidgetHost*)> did_create_callback);

  ~CreateNewPopupWidgetInterceptor() override;

  // LocalFrameHost overrides:
  void CreateNewPopupWidget(
      mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
          blink_popup_widget_host,
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
          blink_widget_host,
      mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget)
      override;

  // LocalFrameHostInterceptorForTesting overrides:
  blink::mojom::LocalFrameHost* GetForwardingInterface() override;

 private:
  mojo::test::ScopedSwapImplForTesting<blink::mojom::LocalFrameHost>
      swapped_impl_;
  base::OnceCallback<void(RenderWidgetHost*)> did_create_callback_;
};

class ShowPopupWidgetWaiter
    : public blink::mojom::PopupWidgetHostInterceptorForTesting {
 public:
  ShowPopupWidgetWaiter(WebContents* web_contents, RenderFrameHost* frame_host);

  ShowPopupWidgetWaiter(const ShowPopupWidgetWaiter&) = delete;
  ShowPopupWidgetWaiter& operator=(const ShowPopupWidgetWaiter&) = delete;

  ~ShowPopupWidgetWaiter() override;

  gfx::Rect last_initial_rect() const { return initial_rect_; }

  int last_routing_id() const { return routing_id_; }

  // Waits until a popup request is received.
  void Wait();

 private:
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  // Helper that waits for a `ShowPopupMenu()` call and then invokes the
  // observer callback with the requested bounds.  The actual call to show the
  // popup menu is treated as if it were cancelled.
  class ShowPopupMenuInterceptor
      : public blink::mojom::LocalFrameHostInterceptorForTesting {
   public:
    explicit ShowPopupMenuInterceptor(RenderFrameHost* rfh,
                                      base::OnceCallback<void(const gfx::Rect&)>
                                          did_show_popup_menu_callback);
    ~ShowPopupMenuInterceptor() override;

    // LocalFrameHost overrides:
    void ShowPopupMenu(
        mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
        const gfx::Rect& bounds,
        double font_size,
        int32_t selected_item,
        std::vector<blink::mojom::MenuItemPtr> menu_items,
        bool right_aligned,
        bool allow_multiple_selection) override;

    // LocalFrameHostInterceptorForTesting overrides:
    blink::mojom::LocalFrameHost* GetForwardingInterface() override;

   private:
    mojo::test::ScopedSwapImplForTesting<blink::mojom::LocalFrameHost>
        swapped_impl_;
    base::OnceCallback<void(const gfx::Rect&)> did_show_popup_menu_callback_;
  };

  void DidShowPopupMenu(const gfx::Rect& bounds);
#endif

  // Callback bound for creating a popup widget.
  void DidCreatePopupWidget(RenderWidgetHost* render_widget_host);

  // blink::mojom::PopupWidgetHostInterceptorForTesting:
  blink::mojom::PopupWidgetHost* GetForwardingInterface() override;
  void ShowPopup(const gfx::Rect& initial_rect,
                 const gfx::Rect& initial_anchor_rect,
                 ShowPopupCallback callback) override;

  CreateNewPopupWidgetInterceptor create_new_popup_widget_interceptor_;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  ShowPopupMenuInterceptor show_popup_menu_interceptor_;
#endif
  base::RunLoop run_loop_;
  gfx::Rect initial_rect_;
  int32_t routing_id_ = IPC::mojom::kRoutingIdNone;
  int32_t process_id_ = 0;
  const raw_ptr<RenderFrameHost> frame_host_;
};

// Intercepts `RequestClosePopup()` method. By default `RequestClosePopup()`
// discards the message. Individual test should override `RequestClosePopup()`
// to customize the behavior.
class RequestCloseWidgetInterceptor
    : public blink::mojom::PopupWidgetHostInterceptorForTesting,
      public RenderWidgetHostObserver {
 public:
  explicit RequestCloseWidgetInterceptor(RenderWidgetHost* render_widget_host);
  ~RequestCloseWidgetInterceptor() override;

  // `blink::mojom::PopupWidgetHostInterceptorForTesting`:
  // Must not be called once the popup widget has been destroyed.
  blink::mojom::PopupWidgetHost* GetForwardingInterface() override;
  void RequestClosePopup() override;

  // `RenderWidgetHostObserver`:
  // The browser can destroy the popup widget without the renderer ever sending
  // `RequestClosePopup()`, e.g. when the owning RenderWidgetHostView loses
  // focus to another views::Widget. Removes the interception then, while the
  // receiver still exists, so this object may safely outlive the widget. Tests
  // waiting for `RequestClosePopup()` should also watch for the widget
  // disappearing.
  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) final;

 private:
  // Engaged while the popup widget is alive.
  std::optional<
      mojo::test::ScopedSwapImplForTesting<blink::mojom::PopupWidgetHost>>
      swapped_impl_;
  base::ScopedObservation<RenderWidgetHost, RenderWidgetHostObserver>
      widget_observation_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_POPUP_TEST_UTILS_H_
