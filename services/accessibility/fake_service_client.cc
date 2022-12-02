// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/fake_service_client.h"

namespace ax {
FakeServiceClient::FakeServiceClient(mojom::AccessibilityService* service)
    : service_(service) {}

FakeServiceClient::~FakeServiceClient() = default;

void FakeServiceClient::BindAutomation(
    mojo::PendingRemote<ax::mojom::Automation> automation,
    mojo::PendingReceiver<ax::mojom::AutomationClient> automation_client) {
  automation_client_receivers_.Add(this, std::move(automation_client));
  automation_remotes_.Add(std::move(automation));
  if (automation_bound_closure_) {
    std::move(automation_bound_closure_).Run();
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

bool FakeServiceClient::AccessibilityServiceClientIsBound() const {
  return a11y_client_receiver_.is_bound();
}

}  // namespace ax
