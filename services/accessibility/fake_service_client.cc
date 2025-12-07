// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/fake_service_client.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ax {
FakeServiceClient::FakeServiceClient(mojom::AccessibilityService* service)
    : service_(service) {
  desktop_tree_id_ = ui::AXTreeID::CreateNewAXTreeID();
}

FakeServiceClient::~FakeServiceClient() = default;

void FakeServiceClient::BindAutomation(
    mojo::PendingAssociatedRemote<ax::mojom::Automation> automation) {
  automation_remotes_.Add(std::move(automation));
  if (automation_bound_closure_) {
    std::move(automation_bound_closure_).Run();
  }
}

void FakeServiceClient::BindAutomationClient(
    mojo::PendingReceiver<ax::mojom::AutomationClient> automation_client) {
  automation_client_receivers_.Add(this, std::move(automation_client));
}

void FakeServiceClient::Enable(EnableCallback callback) {
  std::move(callback).Run(desktop_tree_id_);
}

void FakeServiceClient::Disable() {
  num_disable_called_++;
}

void FakeServiceClient::EnableChildTree(const ui::AXTreeID& tree_id) {}

void FakeServiceClient::PerformAction(const ui::AXActionData& data) {
  if (perform_action_called_callback_) {
    std::move(perform_action_called_callback_).Run(data);
  }
}

void FakeServiceClient::BindAccessibilityServiceClientForTest() {
  if (service_) {
    service_->BindAccessibilityServiceClient(
        a11y_client_receiver_.BindNewPipeAndPassRemote());
  }
}

void FakeServiceClient::SetAutomationBoundClosure(base::OnceClosure closure) {
  automation_bound_closure_ = std::move(closure);
}

bool FakeServiceClient::AutomationIsBound() const {
  return automation_client_receivers_.size() && automation_remotes_.size();
}

void FakeServiceClient::SetPerformActionCalledCallback(
    base::OnceCallback<void(const ui::AXActionData&)> callback) {
  perform_action_called_callback_ = std::move(callback);
}

bool FakeServiceClient::AccessibilityServiceClientIsBound() const {
  return a11y_client_receiver_.is_bound();
}

}  // namespace ax
