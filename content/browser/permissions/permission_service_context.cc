// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_context.h"

#include <utility>

#include "base/bind.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class PermissionServiceContext::PermissionSubscription {
 public:
  PermissionSubscription(
      PermissionServiceContext* context,
      mojo::PendingRemote<blink::mojom::PermissionObserver> observer)
      : context_(context), observer_(std::move(observer)) {
    observer_.set_disconnect_handler(base::BindOnce(
        &PermissionSubscription::OnConnectionError, base::Unretained(this)));
  }
  PermissionSubscription(const PermissionSubscription&) = delete;
  PermissionSubscription& operator=(const PermissionSubscription&) = delete;

  ~PermissionSubscription() {
    DCHECK_NE(id_, 0);
    BrowserContext* browser_context = context_->GetBrowserContext();
    if (browser_context) {
      PermissionControllerImpl::FromBrowserContext(browser_context)
          ->UnsubscribePermissionStatusChange(id_);
    }
  }

  void OnConnectionError() {
    DCHECK_NE(id_, 0);
    context_->ObserverHadConnectionError(id_);
  }

  void OnPermissionStatusChanged(blink::mojom::PermissionStatus status) {
    observer_->OnPermissionStatusChange(status);
  }

  void set_id(int id) { id_ = id; }

 private:
  PermissionServiceContext* const context_;
  mojo::Remote<blink::mojom::PermissionObserver> observer_;
  int id_ = 0;
};

PermissionServiceContext::PermissionServiceContext(
    RenderFrameHost* render_frame_host)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      render_process_host_(nullptr) {
}

PermissionServiceContext::PermissionServiceContext(
    RenderProcessHost* render_process_host)
    : WebContentsObserver(nullptr),
      render_frame_host_(nullptr),
      render_process_host_(render_process_host) {
}

PermissionServiceContext::~PermissionServiceContext() {
}

void PermissionServiceContext::CreateService(
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver) {
  DCHECK(render_frame_host_);
  services_.Add(std::make_unique<PermissionServiceImpl>(
                    this, render_frame_host_->GetLastCommittedOrigin()),
                std::move(receiver));
}

void PermissionServiceContext::CreateServiceForWorker(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver) {
  services_.Add(std::make_unique<PermissionServiceImpl>(this, origin),
                std::move(receiver));
}

void PermissionServiceContext::CreateSubscription(
    PermissionType permission_type,
    const url::Origin& origin,
    blink::mojom::PermissionStatus current_status,
    blink::mojom::PermissionStatus last_known_status,
    mojo::PendingRemote<blink::mojom::PermissionObserver> observer) {
  BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context)
    return;

  auto subscription =
      std::make_unique<PermissionSubscription>(this, std::move(observer));

  if (current_status != last_known_status) {
    subscription->OnPermissionStatusChanged(current_status);
    last_known_status = current_status;
  }

  GURL requesting_origin(origin.Serialize());
  int subscription_id =
      PermissionControllerImpl::FromBrowserContext(browser_context)
          ->SubscribePermissionStatusChange(
              permission_type, render_frame_host_, requesting_origin,
              base::BindRepeating(
                  &PermissionSubscription::OnPermissionStatusChanged,
                  base::Unretained(subscription.get())));
  subscription->set_id(subscription_id);
  subscriptions_[subscription_id] = std::move(subscription);
}

void PermissionServiceContext::ObserverHadConnectionError(int subscription_id) {
  size_t erased = subscriptions_.erase(subscription_id);
  DCHECK_EQ(1u, erased);
}

void PermissionServiceContext::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  CloseBindings(old_host);
}

void PermissionServiceContext::FrameDeleted(
    RenderFrameHost* render_frame_host) {
  CloseBindings(render_frame_host);
}

void PermissionServiceContext::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() || navigation_handle->IsSameDocument())
    return;

  CloseBindings(navigation_handle->GetRenderFrameHost());
}

void PermissionServiceContext::CloseBindings(
    RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host_);
  if (render_frame_host != render_frame_host_)
    return;

  services_.Clear();
  subscriptions_.clear();
}

BrowserContext* PermissionServiceContext::GetBrowserContext() const {
  // web_contents() may return nullptr during teardown, or when showing
  // an interstitial.
  if (web_contents())
    return web_contents()->GetBrowserContext();

  if (render_process_host_)
    return render_process_host_->GetBrowserContext();

  return nullptr;
}

GURL PermissionServiceContext::GetEmbeddingOrigin() const {
  return web_contents() ? web_contents()->GetLastCommittedURL().GetOrigin()
                        : GURL();
}

}  // namespace content
