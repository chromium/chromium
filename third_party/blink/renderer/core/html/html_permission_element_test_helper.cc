// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element_test_helper.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/renderer/core/html/html_capability_element_base.h"

namespace blink {

PermissionStatusChangeWaiter::PermissionStatusChangeWaiter(
    mojo::PendingReceiver<mojom::blink::PermissionObserver> receiver,
    base::OnceClosure callback)
    : receiver_(this, std::move(receiver)), callback_(std::move(callback)) {}

void PermissionStatusChangeWaiter::OnPermissionStatusChange(
    mojom::blink::PermissionStatus status) {
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
  receivers_.Add(this, mojo::PendingReceiver<mojom::blink::PermissionService>(
                           std::move(handle)));
}

void PermissionElementTestPermissionService::HasPermission(
    mojom::blink::PermissionDescriptorPtr permission,
    HasPermissionCallback) {}

void PermissionElementTestPermissionService::
    RegisterPageEmbeddedPermissionControl(
        Vector<mojom::blink::PermissionDescriptorPtr> permissions,
        mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptor,
        mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
            pending_client) {
  if (pepc_registered_callback_) {
    std::move(pepc_registered_callback_).Run();
    return;
  }

  if (should_defer_registered_callback_) {
    pepc_registered_callback_ =
        blink::BindOnce(&PermissionElementTestPermissionService::
                            RegisterPageEmbeddedPermissionControlInternal,
                        blink::Unretained(this), std::move(permissions),
                        std::move(pending_client));
    return;
  }

  RegisterPageEmbeddedPermissionControlInternal(std::move(permissions),
                                                std::move(pending_client));
}

void PermissionElementTestPermissionService::
    RegisterPageEmbeddedPermissionControlInternal(
        Vector<mojom::blink::PermissionDescriptorPtr> permissions,
        mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
            pending_client) {
  Vector<mojom::blink::PermissionStatus> statuses =
      initial_statuses_.empty()
          ? Vector<mojom::blink::PermissionStatus>(
                permissions.size(), mojom::blink::PermissionStatus::ASK)
          : initial_statuses_;
  client_ = mojo::Remote<mojom::blink::EmbeddedPermissionControlClient>(
      std::move(pending_client));
  client_.set_disconnect_handler(
      blink::BindOnce(&PermissionElementTestPermissionService::OnMojoDisconnect,
                      blink::Unretained(this)));
  client_->OnEmbeddedPermissionControlRegistered(/*allow=*/true,
                                                 std::move(statuses));
}

void PermissionElementTestPermissionService::OnMojoDisconnect() {
  if (client_disconnect_run_loop_) {
    client_disconnect_run_loop_->Quit();
  }
}

void PermissionElementTestPermissionService::RequestPageEmbeddedPermission(
    Vector<mojom::blink::PermissionDescriptorPtr> permissions,
    mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptors,
    RequestPageEmbeddedPermissionCallback callback) {
  std::move(callback).Run(
      mojom::blink::EmbeddedPermissionControlResult::kGranted);
}

void PermissionElementTestPermissionService::RequestPermission(
    mojom::blink::PermissionDescriptorPtr permission,
    RequestPermissionCallback) {}

void PermissionElementTestPermissionService::RequestPermissions(
    Vector<mojom::blink::PermissionDescriptorPtr> permissions,
    RequestPermissionsCallback) {}

void PermissionElementTestPermissionService::RevokePermission(
    mojom::blink::PermissionDescriptorPtr permission,
    RevokePermissionCallback) {}

void PermissionElementTestPermissionService::AddPermissionObserver(
    mojom::blink::PermissionDescriptorPtr permission,
    mojom::blink::PermissionStatusWithDetailsPtr last_known_status,
    mojo::PendingRemote<mojom::blink::PermissionObserver> observer) {}

void PermissionElementTestPermissionService::AddPageEmbeddedPermissionObserver(
    mojom::blink::PermissionDescriptorPtr permission,
    mojom::blink::PermissionStatus last_known_status,
    mojo::PendingRemote<mojom::blink::PermissionObserver> observer) {
  observers_.emplace_back(
      permission->name,
      mojo::Remote<mojom::blink::PermissionObserver>(std::move(observer)));
}

void PermissionElementTestPermissionService::NotifyEventListener(
    mojom::blink::PermissionDescriptorPtr permission,
    const String& event_type,
    bool is_added) {}

void PermissionElementTestPermissionService::NotifyPermissionStatusChange(
    mojom::blink::PermissionName name,
    mojom::blink::PermissionStatus status) {
  for (const auto& observer : observers_) {
    if (observer.first == name) {
      observer.second->OnPermissionStatusChange(status);
    }
  }
  WaitForPermissionStatusChange(status);
}

void PermissionElementTestPermissionService::WaitForPermissionStatusChange(
    mojom::blink::PermissionStatus status) {
  mojo::Remote<mojom::blink::PermissionObserver> observer;
  base::RunLoop run_loop;
  auto waiter = std::make_unique<PermissionStatusChangeWaiter>(
      observer.BindNewPipeAndPassReceiver(), run_loop.QuitClosure());
  observer->OnPermissionStatusChange(status);
  run_loop.Run();
}

void PermissionElementTestPermissionService::set_initial_statuses(
    const Vector<mojom::blink::PermissionStatus>& statuses) {
  initial_statuses_ = statuses;
}

void PermissionElementTestPermissionService::WaitForClientDisconnected() {
  client_disconnect_run_loop_ = std::make_unique<base::RunLoop>();
  client_disconnect_run_loop_->Run();
}

void PermissionElementTestPermissionService::set_pepc_registered_callback(
    base::OnceClosure callback) {
  pepc_registered_callback_ = std::move(callback);
}

base::OnceClosure
PermissionElementTestPermissionService::TakePEPCRegisteredCallback() {
  return std::move(pepc_registered_callback_);
}

void PermissionElementTestPermissionService::
    set_should_defer_registered_callback(bool should_defer) {
  should_defer_registered_callback_ = should_defer;
}

void WaitForPermissionElementRegistration(HTMLCapabilityElementBase* el) {
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return el->is_registered_in_browser_process_for_testing(); }));
}

}  // namespace blink
