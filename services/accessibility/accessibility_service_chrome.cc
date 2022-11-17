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

void AccessibilityServiceChrome::BindAccessibilityServiceClient(
    mojo::PendingRemote<mojom::AccessibilityServiceClient>
        accessibility_client_remote) {
  DCHECK(!accessibility_service_client_remote_.is_bound());
  accessibility_service_client_remote_.Bind(
      std::move(accessibility_client_remote));
}

}  // namespace ax
