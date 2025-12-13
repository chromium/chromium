// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FAKE_SERVICE_CLIENT_H_
#define SERVICES_ACCESSIBILITY_FAKE_SERVICE_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/accessibility/buildflags.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/automation.mojom.h"
#include "services/accessibility/public/mojom/automation_client.mojom.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ax {

// A fake AccessibilityServiceClient and AutomationClient for use in tests.
// This allows tests to mock out the OS side of the mojom pipes.
// TODO(b/262637071) This can be extended to allow for passing events into
// the service once the mojom is landed.
// TODO(b/262637071): This should be split for OS vs Browser ATP.
class FakeServiceClient : public mojom::AccessibilityServiceClient,
                          public mojom::AutomationClient {
 public:
  // |service| may be null if it won't be used in the test.
  explicit FakeServiceClient(mojom::AccessibilityService* service);
  FakeServiceClient(const FakeServiceClient& other) = delete;
  FakeServiceClient& operator=(const FakeServiceClient&) = delete;
  ~FakeServiceClient() override;

  // ax::mojom::AccessibilityServiceClient:
  void BindAutomation(
      mojo::PendingAssociatedRemote<ax::mojom::Automation> automation) override;
  void BindAutomationClient(mojo::PendingReceiver<ax::mojom::AutomationClient>
                                automation_client) override;

  // ax::mojom::AutomationClient:
  void Enable(EnableCallback callback) override;
  void Disable() override;
  void EnableChildTree(const ui::AXTreeID& tree_id) override;
  void PerformAction(const ui::AXActionData& data) override;

  // Methods for testing.
  void BindAccessibilityServiceClientForTest();
  bool AccessibilityServiceClientIsBound() const;
  void SetAutomationBoundClosure(base::OnceClosure closure);
  bool AutomationIsBound() const;

  // Runs only once per PerformAction call. This is necessary because we want to
  // check the parameters for each AXActionData.
  void SetPerformActionCalledCallback(
      base::OnceCallback<void(const ui::AXActionData&)> callback);

  base::WeakPtr<FakeServiceClient> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  uint32_t num_disable_called() const { return num_disable_called_; }

 private:
  raw_ptr<mojom::AccessibilityService, DanglingUntriaged> service_;
  base::OnceClosure automation_bound_closure_;
  base::OnceCallback<void(const ui::AXActionData&)>
      perform_action_called_callback_;
  uint32_t num_disable_called_ = 0;

  mojo::AssociatedRemoteSet<mojom::Automation> automation_remotes_;
  mojo::ReceiverSet<mojom::AutomationClient> automation_client_receivers_;

  ui::AXTreeID desktop_tree_id_;
  mojo::Receiver<mojom::AccessibilityServiceClient> a11y_client_receiver_{this};

  base::WeakPtrFactory<FakeServiceClient> weak_ptr_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FAKE_SERVICE_CLIENT_H_
