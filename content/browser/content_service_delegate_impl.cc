// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_service_delegate_impl.h"

#include "base/macros.h"
#include "base/optional.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/content/navigable_contents_delegate.h"
#include "services/content/service.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

namespace {

// Bridge between Content Service navigable contents delegation API and a
// WebContentsImpl.
class NavigableContentsDelegateImpl : public content::NavigableContentsDelegate,
                                      public WebContentsDelegate,
                                      public WebContentsObserver {
 public:
  explicit NavigableContentsDelegateImpl(
      BrowserContext* browser_context,
      const mojom::NavigableContentsParams& params,
      mojom::NavigableContentsClient* client)
      : client_(client),
        enable_view_auto_resize_(params.enable_view_auto_resize),
        auto_resize_min_size_(
            params.auto_resize_min_size.value_or(gfx::Size(1, 1))),
        auto_resize_max_size_(
            params.auto_resize_max_size.value_or(gfx::Size(INT_MAX, INT_MAX))),
        background_color_(params.override_background_color
                              ? base::make_optional(params.background_color)
                              : base::nullopt) {
    WebContents::CreateParams create_params(browser_context);
    web_contents_ = WebContents::Create(create_params);
    WebContentsObserver::Observe(web_contents_.get());
    web_contents_->SetDelegate(this);

    blink::mojom::RendererPreferences* renderer_prefs =
        web_contents_->GetMutableRendererPrefs();
    renderer_prefs->can_accept_load_drops = false;
    renderer_prefs->browser_handles_all_top_level_requests =
        params.suppress_navigations;
    web_contents_->SyncRendererPrefs();
  }

  ~NavigableContentsDelegateImpl() override {
    WebContentsObserver::Observe(nullptr);
  }

  bool TakeFocus(WebContents* source, bool reverse) override {
    client_->ClearViewFocus();
    return true;
  }

 private:
  void NotifyAXTreeChange() {
    auto* rfh = web_contents_->GetMainFrame();
    if (rfh)
      client_->UpdateContentAXTree(rfh->GetAXTreeID());
    else
      client_->UpdateContentAXTree(ui::AXTreeIDUnknown());
  }

  // content::NavigableContentsDelegate:
  gfx::NativeView GetNativeView() override {
    return web_contents_->GetNativeView();
  }

  void Navigate(const GURL& url,
                content::mojom::NavigateParamsPtr params) override {
    NavigationController::LoadURLParams load_url_params(url);
    load_url_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
    load_url_params.should_clear_history_list =
        params->should_clear_session_history;
    web_contents_->GetController().LoadURLWithParams(load_url_params);
  }

  void GoBack(
      content::mojom::NavigableContents::GoBackCallback callback) override {
    content::NavigationController& controller = web_contents_->GetController();
    if (controller.CanGoBack()) {
      std::move(callback).Run(/*success=*/true);
      controller.GoBack();
    } else {
      std::move(callback).Run(/*success=*/false);
    }
  }

  void Focus() override { web_contents_->Focus(); }

  void FocusThroughTabTraversal(bool reverse) override {
    web_contents_->FocusThroughTabTraversal(reverse);
  }

  // WebContentsDelegate:
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override {
    // This method is invoked when attempting to open links in a new tab, e.g.:
    // <a href="https://www.google.com/" target="_blank">Link</a>
    client_->DidSuppressNavigation(target_url,
                                   WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                   /*from_user_gesture=*/true);
    return true;
  }

  WebContents* OpenURLFromTab(WebContents* source,
                              const OpenURLParams& params) override {
    client_->DidSuppressNavigation(params.url, params.disposition,
                                   params.user_gesture);
    return nullptr;
  }

  void ResizeDueToAutoResize(WebContents* web_contents,
                             const gfx::Size& new_size) override {
    DCHECK_EQ(web_contents, web_contents_.get());
    client_->DidAutoResizeView(new_size);
  }

  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override {
    MaybeNotifyCanGoBack();
  }

  // WebContentsObserver:
  void RenderViewReady() override {
    if (background_color_) {
      web_contents_->GetRenderViewHost()
          ->GetWidget()
          ->GetView()
          ->SetBackgroundColor(background_color_.value());
    }
  }

  void RenderViewCreated(RenderViewHost* render_view_host) override {
    if (background_color_) {
      render_view_host->GetWidget()->GetView()->SetBackgroundColor(
          background_color_.value());
    }
  }

  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override {
    if (enable_view_auto_resize_ && web_contents_->GetRenderWidgetHostView()) {
      web_contents_->GetRenderWidgetHostView()->EnableAutoResize(
          auto_resize_min_size_, auto_resize_max_size_);
    }

    if (background_color_) {
      new_host->GetWidget()->GetView()->SetBackgroundColor(
          background_color_.value());
    }

    NotifyAXTreeChange();
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    client_->DidFinishNavigation(
        navigation_handle->GetURL(), navigation_handle->IsInMainFrame(),
        navigation_handle->IsErrorPage(),
        navigation_handle->GetResponseHeaders()
            ? base::MakeRefCounted<net::HttpResponseHeaders>(
                  navigation_handle->GetResponseHeaders()->raw_headers())
            : nullptr);
  }

  void DidStopLoading() override { client_->DidStopLoading(); }

  void NavigationEntriesDeleted() override { MaybeNotifyCanGoBack(); }

  void DidAttachInterstitialPage() override { MaybeNotifyCanGoBack(); }

  void DidDetachInterstitialPage() override { MaybeNotifyCanGoBack(); }

  // Notifies the client whether the web contents can navigate back in its
  // history stack.
  void MaybeNotifyCanGoBack() {
    const bool can_go_back = web_contents_->GetController().CanGoBack();
    if (can_go_back_ == can_go_back)
      return;

    can_go_back_ = can_go_back;
    client_->UpdateCanGoBack(can_go_back);
  }

  void OnFocusChangedInPage(FocusedNodeDetails* details) override {
    client_->FocusedNodeChanged(details->is_editable_node,
                                details->node_bounds_in_screen);
  }

  std::unique_ptr<WebContents> web_contents_;
  mojom::NavigableContentsClient* const client_;

  const bool enable_view_auto_resize_;
  const gfx::Size auto_resize_min_size_;
  const gfx::Size auto_resize_max_size_;
  const base::Optional<SkColor> background_color_;

  bool can_go_back_ = false;

  DISALLOW_COPY_AND_ASSIGN(NavigableContentsDelegateImpl);
};

}  // namespace

ContentServiceDelegateImpl::ContentServiceDelegateImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ContentServiceDelegateImpl::~ContentServiceDelegateImpl() {
  // This delegate is destroyed immediately before |browser_context_| is
  // destroyed. We force-kill any Content Service instances which depend on
  // |this|, since they will no longer be functional anyway.
  std::set<content::Service*> instances;
  std::swap(instances, service_instances_);
  for (content::Service* service : instances) {
    // Eventually destroys |service|. Ensures that no more calls into |this|
    // will occur.
    service->ForceQuit();
  }
}

void ContentServiceDelegateImpl::AddService(content::Service* service) {
  service_instances_.insert(service);
}

void ContentServiceDelegateImpl::WillDestroyServiceInstance(
    content::Service* service) {
  service_instances_.erase(service);
}

std::unique_ptr<content::NavigableContentsDelegate>
ContentServiceDelegateImpl::CreateNavigableContentsDelegate(
    const mojom::NavigableContentsParams& params,
    mojom::NavigableContentsClient* client) {
  return std::make_unique<NavigableContentsDelegateImpl>(browser_context_,
                                                         params, client);
}

}  // namespace content
