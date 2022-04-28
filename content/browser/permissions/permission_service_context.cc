// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_context.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/permissions/permission_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

namespace content {

// A holder owning document-associated PermissionServiceContext. The holder is
// used as PermissionServiceContext itself can't inherit from
// DocumentUserData, as PermissionServiceContext (unlike
// DocumentUserData) can be created when RenderFrameHost doesn't exist
// (e.g. for service workers).
struct PermissionServiceContext::DocumentPermissionServiceContextHolder
    : public DocumentUserData<DocumentPermissionServiceContextHolder> {
  explicit DocumentPermissionServiceContextHolder(RenderFrameHost* rfh)
      : DocumentUserData<DocumentPermissionServiceContextHolder>(rfh),
        permission_service_context(rfh) {}

  PermissionServiceContext permission_service_context;

  DOCUMENT_USER_DATA_KEY_DECL();
};

DOCUMENT_USER_DATA_KEY_IMPL(
    PermissionServiceContext::DocumentPermissionServiceContextHolder);

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
    DCHECK(id_);
    BrowserContext* browser_context = context_->GetBrowserContext();
    if (browser_context) {
      PermissionControllerImpl::FromBrowserContext(browser_context)
          ->UnsubscribePermissionStatusChange(id_);
    }
  }

  void OnConnectionError() {
    DCHECK(id_);
    context_->ObserverHadConnectionError(id_);
  }

  void OnPermissionStatusChanged(blink::mojom::PermissionStatus status) {
    if (observer_.is_connected())
      observer_->OnPermissionStatusChange(status);
  }

  void set_id(PermissionController::SubscriptionId id) { id_ = id; }

 private:
  const raw_ptr<PermissionServiceContext> context_;
  mojo::Remote<blink::mojom::PermissionObserver> observer_;
  PermissionController::SubscriptionId id_;
};

// static
PermissionServiceContext* PermissionServiceContext::GetForCurrentDocument(
    RenderFrameHost* render_frame_host) {
  return &DocumentPermissionServiceContextHolder::GetOrCreateForCurrentDocument(
              render_frame_host)
              ->permission_service_context;
}

PermissionServiceContext::PermissionServiceContext(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host), render_process_host_(nullptr) {
  render_frame_host->GetProcess()->AddObserver(this);
}

PermissionServiceContext::PermissionServiceContext(
    RenderProcessHost* render_process_host)
    : render_frame_host_(nullptr), render_process_host_(render_process_host) {}

PermissionServiceContext::~PermissionServiceContext() {
  if (render_frame_host_)
    render_frame_host_->GetProcess()->RemoveObserver(this);
}

void PermissionServiceContext::CreateService(
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver) {
  DCHECK(render_frame_host_);
  services_.Add(
      std::make_unique<PermissionServiceImpl>(
          this, url::Origin::Create(PermissionUtil::GetLastCommittedOriginAsURL(
                    render_frame_host_))),
      std::move(receiver));
}

void PermissionServiceContext::CreateServiceForWorker(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver) {
  services_.Add(std::make_unique<PermissionServiceImpl>(this, origin),
                std::move(receiver));
}

void PermissionServiceContext::CreateSubscription(
    blink::PermissionType permission_type,
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
  auto subscription_id =
      PermissionControllerImpl::FromBrowserContext(browser_context)
          ->SubscribePermissionStatusChange(
              permission_type, render_process_host_, render_frame_host_,
              requesting_origin,
              base::BindRepeating(
                  &PermissionSubscription::OnPermissionStatusChanged,
                  base::Unretained(subscription.get())));
  subscription->set_id(subscription_id);
  subscriptions_[subscription_id] = std::move(subscription);
}

void PermissionServiceContext::ObserverHadConnectionError(
    PermissionController::SubscriptionId subscription_id) {
  size_t erased = subscriptions_.erase(subscription_id);
  DCHECK_EQ(1u, erased);
}

BrowserContext* PermissionServiceContext::GetBrowserContext() const {
  if (render_frame_host_)
    return render_frame_host_->GetBrowserContext();

  if (render_process_host_)
    return render_process_host_->GetBrowserContext();

  return nullptr;
}

GURL PermissionServiceContext::GetEmbeddingOrigin() const {
  return render_frame_host_ ? PermissionUtil::GetLastCommittedOriginAsURL(
                                  render_frame_host_->GetMainFrame())
                            : GURL();
}

void PermissionServiceContext::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK(host == render_frame_host_->GetProcess());
  subscriptions_.clear();
  // RenderProcessHostImpl will always outlive 'this', but it gets cleaned up
  // earlier so we need to listen to this event so we can do our clean up as
  // well.
  host->RemoveObserver(this);
}

}  // namespace content
