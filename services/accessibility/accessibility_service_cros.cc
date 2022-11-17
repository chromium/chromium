// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/accessibility_service_cros.h"

#include <memory>

#include "services/accessibility/assistive_technology_controller_impl.h"

namespace ax {

AccessibilityServiceCros::AccessibilityServiceCros(
    mojo::PendingReceiver<mojom::AccessibilityService> receiver)
    : receiver_(this, std::move(receiver)) {
  at_controller_ = std::make_unique<AssistiveTechnologyControllerImpl>();
}

AccessibilityServiceCros::~AccessibilityServiceCros() = default;

void AccessibilityServiceCros::BindAccessibilityServiceClient(
    mojo::PendingRemote<mojom::AccessibilityServiceClient>
        accessibility_client_remote) {
  at_controller_->BindAccessibilityServiceClient(
      std::move(accessibility_client_remote));
}

void AccessibilityServiceCros::BindAssistiveTechnologyController(
    mojo::PendingReceiver<mojom::AssistiveTechnologyController>
        at_at_controller_receiver,
    const std::vector<mojom::AssistiveTechnologyType>& enabled_features) {
  at_controller_->Bind(std::move(at_at_controller_receiver));
  at_controller_->EnableAssistiveTechnology(enabled_features);
}

}  // namespace ax
