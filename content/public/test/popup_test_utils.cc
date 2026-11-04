// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/popup_test_utils.h"

#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#endif

namespace content {

CreateNewPopupWidgetInterceptor::CreateNewPopupWidgetInterceptor(
    RenderFrameHost* rfh,
    base::OnceCallback<void(RenderWidgetHost*)> did_create_callback)
    : swapped_impl_(static_cast<RenderFrameHostImpl*>(rfh)
                        ->local_frame_host_receiver_for_testing(),
                    this),
      did_create_callback_(std::move(did_create_callback)) {}

CreateNewPopupWidgetInterceptor::~CreateNewPopupWidgetInterceptor() = default;

void CreateNewPopupWidgetInterceptor::CreateNewPopupWidget(
    mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
        blink_popup_widget_host,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {
  class PopupWidgetCreationObserver : public RenderWidgetHostFactory {
   public:
    PopupWidgetCreationObserver() { RegisterFactory(this); }

    ~PopupWidgetCreationObserver() override { UnregisterFactory(); }

    // RenderWidgetHostFactory overrides:
    RenderWidgetHostImpl* CreateSelfOwnedRenderWidgetHost(
        FrameTree* frame_tree,
        RenderWidgetHostDelegate* delegate,
        base::SafeRef<SiteInstanceGroup> site_instance_group,
        int32_t routing_id,
        bool hidden,
        GlobalRenderFrameHostId popup_creator_frame_id) override {
      CHECK(!last_created_widget_);
      last_created_widget_ =
          RenderWidgetHostFactory::CreateSelfOwnedRenderWidgetHost(
              frame_tree, delegate, std::move(site_instance_group), routing_id,
              hidden, popup_creator_frame_id);
      return last_created_widget_;
    }

    RenderWidgetHostImpl* TakeLastCreatedWidget() {
      return std::exchange(last_created_widget_, nullptr);
    }

   private:
    raw_ptr<RenderWidgetHostImpl> last_created_widget_;
  };

  PopupWidgetCreationObserver creation_observer;

  GetForwardingInterface()->CreateNewPopupWidget(
      std::move(blink_popup_widget_host), std::move(blink_widget_host),
      std::move(blink_widget));

  if (!did_create_callback_) {
    return;
  }

  if (auto* widget = creation_observer.TakeLastCreatedWidget(); widget) {
    std::move(did_create_callback_).Run(widget);
  }
}

blink::mojom::LocalFrameHost*
CreateNewPopupWidgetInterceptor::GetForwardingInterface() {
  return swapped_impl_.old_impl();
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
ShowPopupWidgetWaiter::ShowPopupMenuInterceptor::ShowPopupMenuInterceptor(
    RenderFrameHost* rfh,
    base::OnceCallback<void(const gfx::Rect&)> did_show_popup_menu_callback)
    : swapped_impl_(static_cast<RenderFrameHostImpl*>(rfh)
                        ->local_frame_host_receiver_for_testing(),
                    this),
      did_show_popup_menu_callback_(std::move(did_show_popup_menu_callback)) {}

ShowPopupWidgetWaiter::ShowPopupMenuInterceptor::~ShowPopupMenuInterceptor() =
    default;

void ShowPopupWidgetWaiter::ShowPopupMenuInterceptor::ShowPopupMenu(
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    double font_size,
    int32_t selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  if (did_show_popup_menu_callback_) {
    std::move(did_show_popup_menu_callback_).Run(bounds);
    mojo::Remote<blink::mojom::PopupMenuClient>(std::move(popup_client))
        ->DidCancel();
    return;
  }

  GetForwardingInterface()->ShowPopupMenu(
      std::move(popup_client), bounds, font_size, selected_item,
      std::move(menu_items), right_aligned, allow_multiple_selection);
}

blink::mojom::LocalFrameHost*
ShowPopupWidgetWaiter::ShowPopupMenuInterceptor::GetForwardingInterface() {
  return swapped_impl_.old_impl();
}
#endif

ShowPopupWidgetWaiter::ShowPopupWidgetWaiter(WebContents* web_contents,
                                             RenderFrameHost* frame_host)
    : create_new_popup_widget_interceptor_(
          static_cast<RenderFrameHostImpl*>(frame_host),
          base::BindOnce(&ShowPopupWidgetWaiter::DidCreatePopupWidget,
                         base::Unretained(this))),
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
      show_popup_menu_interceptor_(
          frame_host,
          base::BindOnce(&ShowPopupWidgetWaiter::DidShowPopupMenu,
                         base::Unretained(this))),
#endif

      frame_host_(frame_host) {
}

ShowPopupWidgetWaiter::~ShowPopupWidgetWaiter() {
  if (auto* rwhi = RenderWidgetHostImpl::FromID(process_id_, routing_id_)) {
    std::ignore =
        rwhi->popup_widget_host_receiver_for_testing().SwapImplForTesting(rwhi);
  }
}

void ShowPopupWidgetWaiter::Wait() {
  run_loop_.Run();
}

blink::mojom::PopupWidgetHost* ShowPopupWidgetWaiter::GetForwardingInterface() {
  DCHECK_NE(IPC::mojom::kRoutingIdNone, routing_id_);
  return RenderWidgetHostImpl::FromID(process_id_, routing_id_);
}

void ShowPopupWidgetWaiter::ShowPopup(const gfx::Rect& initial_rect,
                                      const gfx::Rect& initial_anchor_rect,
                                      ShowPopupCallback callback) {
  GetForwardingInterface()->ShowPopup(initial_rect, initial_anchor_rect,
                                      std::move(callback));
  initial_rect_ = initial_rect;
  run_loop_.Quit();
}

void ShowPopupWidgetWaiter::DidCreatePopupWidget(
    RenderWidgetHost* render_widget_host) {
  process_id_ = render_widget_host->GetProcess()->GetDeprecatedID();
  routing_id_ = render_widget_host->GetRoutingID();
  // Swapped back in destructor from process_id_ and routing_id_ lookup.
  std::ignore = static_cast<RenderWidgetHostImpl*>(render_widget_host)
                    ->popup_widget_host_receiver_for_testing()
                    .SwapImplForTesting(this);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
void ShowPopupWidgetWaiter::DidShowPopupMenu(const gfx::Rect& bounds) {
  initial_rect_ = bounds;
  run_loop_.Quit();
}
#endif

RequestCloseWidgetInterceptor::RequestCloseWidgetInterceptor(
    RenderWidgetHost* render_widget_host) {
  swapped_impl_.emplace(static_cast<RenderWidgetHostImpl*>(render_widget_host)
                            ->popup_widget_host_receiver_for_testing(),
                        this);
  widget_observation_.Observe(render_widget_host);
}

RequestCloseWidgetInterceptor::~RequestCloseWidgetInterceptor() = default;

blink::mojom::PopupWidgetHost*
RequestCloseWidgetInterceptor::GetForwardingInterface() {
  // `RenderWidgetHostDestroyed()` removes the interception, so nothing should
  // reach this once the popup widget is gone.
  CHECK(swapped_impl_);
  return swapped_impl_->old_impl();
}

void RequestCloseWidgetInterceptor::RequestClosePopup() {}

void RequestCloseWidgetInterceptor::RenderWidgetHostDestroyed(
    RenderWidgetHost* widget_host) {
  // The widget is going away, taking its PopupWidgetHost receiver with it.
  // Restore the original implementation now, while the receiver still exists,
  // so the destructor doesn't touch freed memory.
  widget_observation_.Reset();
  swapped_impl_.reset();
}

}  // namespace content
