// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_service_delegate_impl.h"

#include "base/macros.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/renderer_preferences.h"
#include "services/content/navigable_contents_delegate.h"
#include "services/content/service.h"

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
        enable_view_auto_resize_(params.enable_view_auto_resize) {
    WebContents::CreateParams create_params(browser_context);
    web_contents_ = WebContents::Create(create_params);
    WebContentsObserver::Observe(web_contents_.get());
    web_contents_->SetDelegate(this);

    content::RendererPreferences* renderer_prefs =
        web_contents_->GetMutableRendererPrefs();
    renderer_prefs->can_accept_load_drops = false;
    renderer_prefs->browser_handles_all_top_level_requests =
        params.suppress_navigations;
    web_contents_->GetRenderViewHost()->SyncRendererPrefs();
  }

  ~NavigableContentsDelegateImpl() override {
    WebContentsObserver::Observe(nullptr);
  }

 private:
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

  // WebContentsDelegate:
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

  // WebContentsObserver:
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override {
    if (enable_view_auto_resize_ && web_contents_->GetRenderWidgetHostView()) {
      web_contents_->GetRenderWidgetHostView()->EnableAutoResize(
          gfx::Size(1, 1), gfx::Size(INT_MAX, INT_MAX));
    }
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

  std::unique_ptr<WebContents> web_contents_;
  mojom::NavigableContentsClient* const client_;

  const bool enable_view_auto_resize_;

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
