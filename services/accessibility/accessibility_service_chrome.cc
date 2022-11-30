// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/accessibility_service_chrome.h"

#include <memory>
#include "services/accessibility/automation_impl.h"

namespace ax {

AccessibilityServiceChrome::AccessibilityServiceChrome(
    mojo::PendingReceiver<mojom::AccessibilityService> receiver)
    : receiver_(this, std::move(receiver)) {
  automation_ = std::make_unique<AutomationImpl>();
}

AccessibilityServiceChrome::~AccessibilityServiceChrome() = default;

void AccessibilityServiceChrome::BindAutomation(
    mojo::PendingRemote<mojom::AutomationClient> automation_client_remote,
    mojo::PendingReceiver<mojom::Automation> automation_receiver) {
  automation_->Bind(std::move(automation_client_remote),
                    std::move(automation_receiver));
}

}  // namespace ax
