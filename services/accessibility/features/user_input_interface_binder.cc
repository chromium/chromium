// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/user_input_interface_binder.h"

#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/user_input.mojom.h"

namespace ax {

UserInputInterfaceBinder::UserInputInterfaceBinder(
    mojom::AccessibilityServiceClient* ax_service_client)
    : ax_service_client_(ax_service_client) {}

UserInputInterfaceBinder::~UserInputInterfaceBinder() = default;

bool UserInputInterfaceBinder::MatchesInterface(
    const std::string& interface_name) {
  return interface_name == "ax.mojom.UserInput";
}

void UserInputInterfaceBinder::BindReceiver(
    mojo::GenericPendingReceiver receiver) {
  ax_service_client_->BindUserInput(receiver.As<ax::mojom::UserInput>());
}

}  // namespace ax
