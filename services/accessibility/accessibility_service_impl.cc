// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/accessibility_service_impl.h"

#include <memory>
#include "services/accessibility/automation_impl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/accessibility/assistive_technology_controller_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace ax {

AccessibilityServiceImpl::AccessibilityServiceImpl(
    mojo::PendingReceiver<mojom::AccessibilityService> receiver)
    : receiver_(this, std::move(receiver)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  at_controller_ = std::make_unique<AssistiveTechnologyControllerImpl>();
#endif
  automation_ = std::make_unique<AutomationImpl>();
}

AccessibilityServiceImpl::~AccessibilityServiceImpl() = default;

void AccessibilityServiceImpl::BindAutomation(
    mojo::PendingRemote<mojom::AutomationClient> automation_client_remote,
    mojo::PendingReceiver<mojom::Automation> automation_receiver) {
  automation_->Bind(std::move(automation_client_remote),
                    std::move(automation_receiver));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AccessibilityServiceImpl::BindAssistiveTechnologyController(
    mojo::PendingReceiver<mojom::AssistiveTechnologyController>
        at_at_controller_receiver) {
  at_controller_->Bind(std::move(at_at_controller_receiver));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace ax