// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/os_accessibility_service.h"

#include <memory>

#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/features/v8_manager.h"

namespace ax {

OSAccessibilityService::OSAccessibilityService(
    mojo::PendingReceiver<mojom::AccessibilityService> receiver)
    : receiver_(this, std::move(receiver)) {
  at_controller_ = std::make_unique<AssistiveTechnologyControllerImpl>();
}

OSAccessibilityService::~OSAccessibilityService() = default;

void OSAccessibilityService::BindAccessibilityServiceClient(
    mojo::PendingRemote<mojom::AccessibilityServiceClient>
        accessibility_client_remote) {
  at_controller_->BindAccessibilityServiceClient(
      std::move(accessibility_client_remote));
}

void OSAccessibilityService::BindAssistiveTechnologyController(
    mojo::PendingReceiver<mojom::AssistiveTechnologyController>
        at_at_controller_receiver,
    const std::vector<mojom::AssistiveTechnologyType>& enabled_features) {
  at_controller_->Bind(std::move(at_at_controller_receiver));
  at_controller_->EnableAssistiveTechnology(enabled_features);
}

void OSAccessibilityService::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<::blink::mojom::DevToolsAgent> agent,
    mojom::AssistiveTechnologyType type) {
  auto& manager = at_controller_->GetOrCreateV8Manager(type);
  manager.ConnectDevToolsAgent(std::move(agent));
}

}  // namespace ax
