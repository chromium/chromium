// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_permission_controller.h"

#include <utility>

namespace content {

namespace {

class MockPermissionSubscription
    : public PermissionController::PermissionSubscription {
 public:
  MockPermissionSubscription(base::WeakPtr<MockPermissionController> controller,
                             PermissionController::SubscriptionId id)
      : controller_(std::move(controller)), id_(id) {}

  ~MockPermissionSubscription() override {
    if (!id_.is_null() && controller_) {
      controller_->UnsubscribeFromPermissionResultChange(id_);
    }
  }

 private:
  base::WeakPtr<MockPermissionController> controller_;
  PermissionController::SubscriptionId id_;
};

}  // namespace

MockPermissionController::MockPermissionController() = default;

MockPermissionController::~MockPermissionController() = default;

std::unique_ptr<PermissionController::PermissionSubscription>
MockPermissionController::CreateSubscription(SubscriptionId id) {
  return std::make_unique<MockPermissionSubscription>(
      weak_factory_.GetWeakPtr(), id);
}

}  // namespace content
