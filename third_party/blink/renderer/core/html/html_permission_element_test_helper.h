// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_TEST_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_TEST_HELPER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"

namespace base {
class RunLoop;
}  // namespace base

namespace blink {

class HTMLCapabilityElementBase;

// Helper class used to wait until receiving a permission status change event.
class PermissionStatusChangeWaiter : public mojom::blink::PermissionObserver {
 public:
  explicit PermissionStatusChangeWaiter(
      mojo::PendingReceiver<mojom::blink::PermissionObserver> receiver,
      base::OnceClosure callback);

  // mojom::blink::PermissionObserver override
  void OnPermissionStatusChange(mojom::blink::PermissionStatus status) override;

 private:
  mojo::Receiver<mojom::blink::PermissionObserver> receiver_;
  base::OnceClosure callback_;
};

class PermissionElementTestPermissionService
    : public mojom::blink::PermissionService {
 public:
  explicit PermissionElementTestPermissionService();
  ~PermissionElementTestPermissionService() override;

  void BindHandle(mojo::ScopedMessagePipeHandle handle);

  // mojom::blink::PermissionService implementation
  void HasPermission(mojom::blink::PermissionDescriptorPtr permission,
                     HasPermissionCallback) override;
  void RegisterPageEmbeddedPermissionControl(
      Vector<mojom::blink::PermissionDescriptorPtr> permissions,
      mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptor,
      mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
          pending_client) override;

  void RequestPageEmbeddedPermission(
      Vector<mojom::blink::PermissionDescriptorPtr> permissions,
      mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptors,
      RequestPageEmbeddedPermissionCallback callback) override;
  void RequestPermission(mojom::blink::PermissionDescriptorPtr permission,
                         bool user_gesture,
                         RequestPermissionCallback) override;
  void RequestPermissions(
      Vector<mojom::blink::PermissionDescriptorPtr> permissions,
      bool user_gesture,
      RequestPermissionsCallback) override;
  void RevokePermission(mojom::blink::PermissionDescriptorPtr permission,
                        RevokePermissionCallback) override;
  void AddPermissionObserver(
      mojom::blink::PermissionDescriptorPtr permission,
      mojom::blink::PermissionStatus last_known_status,
      mojo::PendingRemote<mojom::blink::PermissionObserver> observer) override;
  void AddPageEmbeddedPermissionObserver(
      mojom::blink::PermissionDescriptorPtr permission,
      mojom::blink::PermissionStatus last_known_status,
      mojo::PendingRemote<mojom::blink::PermissionObserver> observer) override;

  void NotifyEventListener(mojom::blink::PermissionDescriptorPtr permission,
                           const String& event_type,
                           bool is_added) override;

  void NotifyPermissionStatusChange(mojom::blink::PermissionName name,
                                    mojom::blink::PermissionStatus status);

  void WaitForPermissionStatusChange(mojom::blink::PermissionStatus status);

  void set_initial_statuses(
      const Vector<mojom::blink::PermissionStatus>& statuses);

  void WaitForClientDisconnected();
  void set_pepc_registered_callback(base::OnceClosure callback);
  base::OnceClosure TakePEPCRegisteredCallback();
  void set_should_defer_registered_callback(bool should_defer);

 private:
  void OnMojoDisconnect();
  void RegisterPageEmbeddedPermissionControlInternal(
      Vector<mojom::blink::PermissionDescriptorPtr> permissions,
      mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
          pending_client);

  mojo::ReceiverSet<mojom::blink::PermissionService> receivers_;
  Vector<std::pair<mojom::blink::PermissionName,
                   mojo::Remote<mojom::blink::PermissionObserver>>>
      observers_;
  Vector<mojom::blink::PermissionStatus> initial_statuses_;
  mojo::Remote<mojom::blink::EmbeddedPermissionControlClient> client_;
  bool should_defer_registered_callback_ = false;
  base::OnceClosure pepc_registered_callback_;
  std::unique_ptr<base::RunLoop> client_disconnect_run_loop_;
};

void WaitForPermissionElementRegistration(HTMLCapabilityElementBase*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_TEST_HELPER_H_
