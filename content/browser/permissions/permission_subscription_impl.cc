// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_subscription_impl.h"

#include <utility>

#include "content/browser/permissions/permission_controller_impl.h"

namespace content {

PermissionSubscriptionImpl::PermissionSubscriptionImpl(
    base::WeakPtr<PermissionControllerImpl> controller,
    PermissionController::SubscriptionId id)
    : controller_(std::move(controller)), id_(id) {}

PermissionSubscriptionImpl::~PermissionSubscriptionImpl() {
  if (id_.is_null()) {
    return;
  }
  // The controller may already be gone during shutdown, in which case the
  // subscription went away with it.
  if (controller_) {
    controller_->UnsubscribeFromPermissionResultChange(id_);
  }
}

PermissionController::SubscriptionId PermissionSubscriptionImpl::Release() {
  controller_.reset();
  return std::exchange(id_, PermissionController::SubscriptionId());
}

}  // namespace content
