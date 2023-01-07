// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/accessibility_service_cros.h"

#include <memory>
#include "services/accessibility/automation_impl.h"

#include "services/accessibility/assistive_technology_controller_impl.h"

namespace ax {

AccessibilityServiceCros::AccessibilityServiceCros(
    mojo::PendingReceiver<mojom::AccessibilityService> receiver)
    : receiver_(this, std::move(receiver)) {
  at_controller_ = std::make_unique<AssistiveTechnologyControllerImpl>();
  automation_ = std::make_unique<AutomationImpl>();
}

AccessibilityServiceCros::~AccessibilityServiceCros() = default;

void AccessibilityServiceCros::BindAutomation(
    mojo::PendingRemote<mojom::AutomationClient> automation_client_remote,
    mojo::PendingReceiver<mojom::Automation> automation_receiver) {
  automation_->Bind(std::move(automation_client_remote),
                    std::move(automation_receiver));
}

void AccessibilityServiceCros::BindAssistiveTechnologyController(
    mojo::PendingReceiver<mojom::AssistiveTechnologyController>
        at_at_controller_receiver,
    const std::vector<mojom::AssistiveTechnologyType>& enabled_features) {
  at_controller_->Bind(std::move(at_at_controller_receiver));
  for (auto feature : enabled_features) {
    at_controller_->EnableAssistiveTechnology(feature, /*enabled=*/true);
  }
}

}  // namespace ax
