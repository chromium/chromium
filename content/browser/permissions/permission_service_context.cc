// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_context.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
      PermissionResult last_known_result,
      PermissionServiceContext* context,
      mojo::PendingRemote<blink::mojom::PermissionObserver> observer)
      : last_known_result_(last_known_result),
        context_(context),
        observer_(std::move(observer)) {
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
          ->UnsubscribeFromPermissionResultChange(id_);
    }
  }

  void OnConnectionError() {
    DCHECK(id_);
    context_->ObserverHadConnectionError(id_);
  }

  void StoreResultAtBFCacheEntry() {
    result_at_bf_cache_entry_ = last_known_result_;
  }

  void NotifyPermissionResultChangedIfNeeded() {
    DCHECK(result_at_bf_cache_entry_.has_value());
    if (result_at_bf_cache_entry_ != last_known_result_) {
      observer_->OnPermissionStatusChange(last_known_result_.status);
    }
    result_at_bf_cache_entry_.reset();
  }

  void OnPermissionStatusChanged(PermissionResult permission_result) {
    if (!observer_.is_connected()) {
      return;
    }

    last_known_result_ = permission_result;

    // Dispatching events while in BFCache is redundant. Permissions code in
    // renderer process would decide to drop the event by looking at document's
    // active status.
    if (!result_at_bf_cache_entry_.has_value()) {
      observer_->OnPermissionStatusChange(permission_result.status);
    }
  }

  void set_id(PermissionController::SubscriptionId id) { id_ = id; }

  base::WeakPtr<PermissionSubscription> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  PermissionResult last_known_result_ =
      PermissionResult(PermissionStatus::LAST);
  const raw_ptr<PermissionServiceContext> context_;
  mojo::Remote<blink::mojom::PermissionObserver> observer_;
  PermissionController::SubscriptionId id_;

  // Optional variable to store the last status before the corresponding
  // RenderFrameHost enters  BFCache, and will be cleared when the
  // RenderFrameHost is restored from BFCache. Non-empty value indicates that
  // the RenderFrameHost is in BFCache.
  std::optional<PermissionResult> result_at_bf_cache_entry_;
  base::WeakPtrFactory<PermissionSubscription> weak_ptr_factory_{this};
};

// static
PermissionServiceContext*
PermissionServiceContext::GetOrCreateForCurrentDocument(
    RenderFrameHost* render_frame_host) {
  return &DocumentPermissionServiceContextHolder::GetOrCreateForCurrentDocument(
              render_frame_host)
              ->permission_service_context;
}

// static
PermissionServiceContext* PermissionServiceContext::GetForCurrentDocument(
    RenderFrameHost* render_frame_host) {
  auto* holder = DocumentPermissionServiceContextHolder::GetForCurrentDocument(
      render_frame_host);
  return holder ? &holder->permission_service_context : nullptr;
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
  if (render_frame_host_) {
    render_frame_host_->GetProcess()->RemoveObserver(this);
  }
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
    const blink::mojom::PermissionDescriptorPtr& permission,
    const url::Origin& origin,
    PermissionResult current_result,
    PermissionResult last_known_result,
    bool should_include_device_status,
    mojo::PendingRemote<blink::mojom::PermissionObserver> observer) {
  BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  auto subscription = std::make_unique<PermissionSubscription>(
      last_known_result, this, std::move(observer));

  if (current_result != last_known_result) {
    subscription->OnPermissionStatusChanged(current_result);
  }

  if (render_frame_host_ &&
      render_frame_host_->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kInBackForwardCache)) {
    subscription->StoreResultAtBFCacheEntry();
  }

  GURL requesting_origin(origin.Serialize());
  auto subscription_id =
      PermissionControllerImpl::FromBrowserContext(browser_context)
          ->SubscribeToPermissionResultChange(
              permission->Clone(), render_process_host_, render_frame_host_,
              requesting_origin, should_include_device_status,
              base::BindRepeating(
                  &PermissionSubscription::OnPermissionStatusChanged,
                  subscription->GetWeakPtr()));
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

std::optional<GURL> PermissionServiceContext::GetEmbeddingOrigin() const {
  if (render_frame_host_) {
    GURL origin_as_url(PermissionUtil::GetLastCommittedOriginAsURL(
        render_frame_host_->GetMainFrame()));
    if (!origin_as_url.is_empty()) {
      return origin_as_url;
    }
  }
  return std::nullopt;
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

void PermissionServiceContext::StoreResultAtBFCacheEntry() {
  for (auto& iter : subscriptions_) {
    iter.second->StoreResultAtBFCacheEntry();
  }
}

void PermissionServiceContext::NotifyPermissionResultChangedIfNeeded() {
  for (auto& iter : subscriptions_) {
    iter.second->NotifyPermissionResultChangedIfNeeded();
  }
}

}  // namespace content
