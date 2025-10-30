// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element_test_helper.h"

#include "base/run_loop.h"

namespace blink {

PermissionStatusChangeWaiter::PermissionStatusChangeWaiter(
    mojo::PendingReceiver<PermissionObserver> receiver,
    base::OnceClosure callback)
    : receiver_(this, std::move(receiver)), callback_(std::move(callback)) {}

void PermissionStatusChangeWaiter::OnPermissionStatusChange(
    MojoPermissionStatus status) {
  if (callback_) {
    std::move(callback_).Run();
  }
}

PermissionElementTestPermissionService::
    PermissionElementTestPermissionService() = default;
PermissionElementTestPermissionService::
    ~PermissionElementTestPermissionService() = default;

void PermissionElementTestPermissionService::BindHandle(
    mojo::ScopedMessagePipeHandle handle) {
  receivers_.Add(this,
                 mojo::PendingReceiver<PermissionService>(std::move(handle)));
}

void PermissionElementTestPermissionService::HasPermission(
    PermissionDescriptorPtr permission,
    HasPermissionCallback) {}

void PermissionElementTestPermissionService::
    RegisterPageEmbeddedPermissionControl(
        Vector<PermissionDescriptorPtr> permissions,
        mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptor,
        mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
            pending_client) {
  Vector<MojoPermissionStatus> statuses =
      initial_statuses_.empty()
          ? Vector<MojoPermissionStatus>(permissions.size(),
                                         MojoPermissionStatus::ASK)
          : initial_statuses_;
  client_ = mojo::Remote<mojom::blink::EmbeddedPermissionControlClient>(
      std::move(pending_client));
  client_->OnEmbeddedPermissionControlRegistered(/*allow=*/true,
                                                 std::move(statuses));
}

void PermissionElementTestPermissionService::RequestPageEmbeddedPermission(
    Vector<PermissionDescriptorPtr> permissions,
    mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptors,
    RequestPageEmbeddedPermissionCallback callback) {
  std::move(callback).Run(
      mojom::blink::EmbeddedPermissionControlResult::kGranted);
}

void PermissionElementTestPermissionService::RequestPermission(
    PermissionDescriptorPtr permission,
    bool user_gesture,
    RequestPermissionCallback) {}

void PermissionElementTestPermissionService::RequestPermissions(
    Vector<PermissionDescriptorPtr> permissions,
    bool user_gesture,
    RequestPermissionsCallback) {}

void PermissionElementTestPermissionService::RevokePermission(
    PermissionDescriptorPtr permission,
    RevokePermissionCallback) {}

void PermissionElementTestPermissionService::AddPermissionObserver(
    PermissionDescriptorPtr permission,
    MojoPermissionStatus last_known_status,
    mojo::PendingRemote<PermissionObserver> observer) {}

void PermissionElementTestPermissionService::AddPageEmbeddedPermissionObserver(
    PermissionDescriptorPtr permission,
    MojoPermissionStatus last_known_status,
    mojo::PendingRemote<PermissionObserver> observer) {
  observers_.emplace_back(
      permission->name, mojo::Remote<PermissionObserver>(std::move(observer)));
}

void PermissionElementTestPermissionService::NotifyEventListener(
    PermissionDescriptorPtr permission,
    const String& event_type,
    bool is_added) {}

void PermissionElementTestPermissionService::NotifyPermissionStatusChange(
    PermissionName name,
    MojoPermissionStatus status) {
  for (const auto& observer : observers_) {
    if (observer.first == name) {
      observer.second->OnPermissionStatusChange(status);
    }
  }
  WaitForPermissionStatusChange(status);
}

void PermissionElementTestPermissionService::WaitForPermissionStatusChange(
    MojoPermissionStatus status) {
  mojo::Remote<PermissionObserver> observer;
  base::RunLoop run_loop;
  auto waiter = std::make_unique<PermissionStatusChangeWaiter>(
      observer.BindNewPipeAndPassReceiver(), run_loop.QuitClosure());
  observer->OnPermissionStatusChange(status);
  run_loop.Run();
}

void PermissionElementTestPermissionService::set_initial_statuses(
    const Vector<MojoPermissionStatus>& statuses) {
  initial_statuses_ = statuses;
}

}  // namespace blink
