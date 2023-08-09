// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/user_interface_interface_binder.h"

#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/user_interface.mojom.h"

namespace ax {

UserInterfaceInterfaceBinder::UserInterfaceInterfaceBinder(
    mojom::AccessibilityServiceClient* ax_service_client)
    : ax_service_client_(ax_service_client) {}

UserInterfaceInterfaceBinder::~UserInterfaceInterfaceBinder() = default;

bool UserInterfaceInterfaceBinder::MatchesInterface(
    const std::string& interface_name) {
  return interface_name == "ax.mojom.UserInterface";
}

void UserInterfaceInterfaceBinder::BindReceiver(
    mojo::GenericPendingReceiver ux_receiver) {
  ax_service_client_->BindUserInterface(
      ux_receiver.As<ax::mojom::UserInterface>());
}

}  // namespace ax
