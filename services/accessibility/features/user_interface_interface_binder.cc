// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/user_interface_interface_binder.h"

#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/user_interface.mojom.h"

namespace ax {

UserInterfaceInterfaceBinder::UserInterfaceInterfaceBinder(
    base::WeakPtr<mojom::AccessibilityServiceClient> ax_service_client,
    scoped_refptr<base::SequencedTaskRunner> main_runner)
    : ax_service_client_(ax_service_client),
      main_runner_(std::move(main_runner)) {}

UserInterfaceInterfaceBinder::~UserInterfaceInterfaceBinder() = default;

bool UserInterfaceInterfaceBinder::MatchesInterface(
    const std::string& interface_name) {
  return interface_name == "ax.mojom.UserInterface";
}

void UserInterfaceInterfaceBinder::BindReceiver(
    mojo::GenericPendingReceiver receiver) {
  CHECK(main_runner_);
  auto ui_receiver = receiver.As<ax::mojom::UserInterface>();
  // This method was called from the V8 thread when JS tried to bind one
  // end of a Mojo pipe. Do the actual binding on the service main thread,
  // where the AccessibilityServiceClient lives.
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<mojom::AccessibilityServiceClient> ax_service_client,
             mojo::PendingReceiver<ax::mojom::UserInterface> ui_receiver) {
            ax_service_client->BindUserInterface(std::move(ui_receiver));
          },
          ax_service_client_, std::move(ui_receiver)));
}

}  // namespace ax
