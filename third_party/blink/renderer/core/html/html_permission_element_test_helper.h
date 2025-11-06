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

class HTMLPermissionElement;

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionObserver;
using mojom::blink::PermissionService;
using MojoPermissionStatus = mojom::blink::PermissionStatus;

// Helper class used to wait until receiving a permission status change event.
class PermissionStatusChangeWaiter : public PermissionObserver {
 public:
  explicit PermissionStatusChangeWaiter(
      mojo::PendingReceiver<PermissionObserver> receiver,
      base::OnceClosure callback);

  // PermissionObserver override
  void OnPermissionStatusChange(MojoPermissionStatus status) override;

 private:
  mojo::Receiver<PermissionObserver> receiver_;
  base::OnceClosure callback_;
};

class PermissionElementTestPermissionService : public PermissionService {
 public:
  explicit PermissionElementTestPermissionService();
  ~PermissionElementTestPermissionService() override;

  void BindHandle(mojo::ScopedMessagePipeHandle handle);

  // mojom::blink::PermissionService implementation
  void HasPermission(PermissionDescriptorPtr permission,
                     HasPermissionCallback) override;
  void RegisterPageEmbeddedPermissionControl(
      Vector<PermissionDescriptorPtr> permissions,
      mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptor,
      mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
          pending_client) override;

  void RequestPageEmbeddedPermission(
      Vector<PermissionDescriptorPtr> permissions,
      mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptors,
      RequestPageEmbeddedPermissionCallback callback) override;
  void RequestPermission(PermissionDescriptorPtr permission,
                         bool user_gesture,
                         RequestPermissionCallback) override;
  void RequestPermissions(Vector<PermissionDescriptorPtr> permissions,
                          bool user_gesture,
                          RequestPermissionsCallback) override;
  void RevokePermission(PermissionDescriptorPtr permission,
                        RevokePermissionCallback) override;
  void AddPermissionObserver(
      PermissionDescriptorPtr permission,
      MojoPermissionStatus last_known_status,
      mojo::PendingRemote<PermissionObserver> observer) override;
  void AddPageEmbeddedPermissionObserver(
      PermissionDescriptorPtr permission,
      MojoPermissionStatus last_known_status,
      mojo::PendingRemote<PermissionObserver> observer) override;

  void NotifyEventListener(PermissionDescriptorPtr permission,
                           const String& event_type,
                           bool is_added) override;

  void NotifyPermissionStatusChange(PermissionName name,
                                    MojoPermissionStatus status);

  void WaitForPermissionStatusChange(MojoPermissionStatus status);

  void set_initial_statuses(const Vector<MojoPermissionStatus>& statuses);

  void WaitForClientDisconnected();
  void set_pepc_registered_callback(base::OnceClosure callback);
  base::OnceClosure TakePEPCRegisteredCallback();
  void set_should_defer_registered_callback(bool should_defer);

 private:
  void OnMojoDisconnect();
  void RegisterPageEmbeddedPermissionControlInternal(
      Vector<PermissionDescriptorPtr> permissions,
      mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
          pending_client);

  mojo::ReceiverSet<PermissionService> receivers_;
  Vector<std::pair<PermissionName, mojo::Remote<PermissionObserver>>>
      observers_;
  Vector<MojoPermissionStatus> initial_statuses_;
  mojo::Remote<mojom::blink::EmbeddedPermissionControlClient> client_;
  bool should_defer_registered_callback_ = false;
  base::OnceClosure pepc_registered_callback_;
  std::unique_ptr<base::RunLoop> client_disconnect_run_loop_;
};

void WaitForPermissionElementRegistration(HTMLPermissionElement*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_TEST_HELPER_H_
