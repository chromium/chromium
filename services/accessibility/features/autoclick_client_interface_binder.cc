// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/autoclick_client_interface_binder.h"

#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/autoclick.mojom.h"

namespace ax {

AutoclickClientInterfaceBinder::AutoclickClientInterfaceBinder(
    mojom::AccessibilityServiceClient* ax_service_client)
    : ax_service_client_(ax_service_client) {}

AutoclickClientInterfaceBinder::~AutoclickClientInterfaceBinder() = default;

bool AutoclickClientInterfaceBinder::MatchesInterface(
    const std::string& interface_name) {
  return interface_name == "ax.mojom.AutoclickClient";
}

void AutoclickClientInterfaceBinder::BindReceiver(
    mojo::GenericPendingReceiver autoclick_receiver) {
  ax_service_client_->BindAutoclickClient(
      autoclick_receiver.As<ax::mojom::AutoclickClient>());
}

}  // namespace ax
