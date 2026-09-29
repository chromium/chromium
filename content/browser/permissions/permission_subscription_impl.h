// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_SUBSCRIPTION_IMPL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_SUBSCRIPTION_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/permission_controller.h"

namespace content {

class PermissionControllerImpl;

class CONTENT_EXPORT PermissionSubscriptionImpl
    : public PermissionController::PermissionSubscription {
 public:
  PermissionSubscriptionImpl(base::WeakPtr<PermissionControllerImpl> controller,
                             PermissionController::SubscriptionId id);

  PermissionSubscriptionImpl(const PermissionSubscriptionImpl&) = delete;
  PermissionSubscriptionImpl& operator=(const PermissionSubscriptionImpl&) =
      delete;

  ~PermissionSubscriptionImpl() override;

  // Returns the id of the owned subscription. Useful for callers that need to
  // key their own bookkeeping by subscription.
  PermissionController::SubscriptionId id() const { return id_; }

 private:
  // MediaStreamManager creates its subscriptions on the UI thread but tracks
  // their ids in DeviceRequest on the IO thread, so no single handle can own
  // them. It is the only permitted user of Release().
  // TODO(crbug.com/40056329): Give MediaStreamManager a UI-thread-owned map of
  // handles keyed by request label, then drop Release() and this friendship.
  friend class MediaStreamManager;

  // Relinquishes ownership of the subscription and returns its id, leaving this
  // handle empty. The caller becomes responsible for calling
  // PermissionControllerImpl::UnsubscribeFromPermissionResultChange().
  //
  // Private on purpose: forgetting that call is the leak this class exists to
  // prevent, so new code must hold the handle instead.
  [[nodiscard]] PermissionController::SubscriptionId Release();

  base::WeakPtr<PermissionControllerImpl> controller_;
  PermissionController::SubscriptionId id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_SUBSCRIPTION_IMPL_H_
