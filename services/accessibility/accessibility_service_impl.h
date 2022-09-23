// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ACCESSIBILITY_SERVICE_IMPL_H_
#define SERVICES_ACCESSIBILITY_ACCESSIBILITY_SERVICE_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {

class AutomationImpl;

#if BUILDFLAG(IS_CHROMEOS_ASH)
class AssistiveTechnologyControllerImpl;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Implementation of the Accessibility Service.
// TODO(crbug.com/1355633): Split into separate implementations for Chrome
// browser and Chrome OS.
class AccessibilityServiceImpl : public mojom::AccessibilityService {
 public:
  explicit AccessibilityServiceImpl(
      mojo::PendingReceiver<mojom::AccessibilityService> receiver);
  ~AccessibilityServiceImpl() override;
  AccessibilityServiceImpl(const AccessibilityServiceImpl&) = delete;
  AccessibilityServiceImpl& operator=(const AccessibilityServiceImpl&) = delete;

 private:
  // mojom::AccessibilityService:
  void BindAutomation(
      mojo::PendingRemote<mojom::AutomationClient> accessibility_client_remote,
      mojo::PendingReceiver<mojom::Automation> automation_receiver) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void BindAssistiveTechnologyController(
      mojo::PendingReceiver<mojom::AssistiveTechnologyController>
          at_controller_receiver) override;

  std::unique_ptr<AssistiveTechnologyControllerImpl> at_controller_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::unique_ptr<AutomationImpl> automation_;

  mojo::Receiver<mojom::AccessibilityService> receiver_;

  base::WeakPtrFactory<AccessibilityServiceImpl> weak_ptr_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_ACCESSIBILITY_SERVICE_IMPL_H_