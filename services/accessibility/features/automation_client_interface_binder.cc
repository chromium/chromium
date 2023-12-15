// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/automation_client_interface_binder.h"

#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/automation_client.mojom.h"

namespace ax {

AutomationClientInterfaceBinder::AutomationClientInterfaceBinder(
    mojom::AccessibilityServiceClient* ax_service_client)
    : ax_service_client_(ax_service_client) {}

AutomationClientInterfaceBinder::~AutomationClientInterfaceBinder() = default;

bool AutomationClientInterfaceBinder::MatchesInterface(
    const std::string& interface_name) {
  return interface_name == "ax.mojom.AutomationClient";
}

void AutomationClientInterfaceBinder::BindReceiver(
    mojo::GenericPendingReceiver automation_client_receiver) {
  ax_service_client_->BindAutomationClient(
      automation_client_receiver.As<ax::mojom::AutomationClient>());
}

}  // namespace ax
