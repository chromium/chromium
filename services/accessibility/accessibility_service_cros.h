// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ACCESSIBILITY_SERVICE_CROS_H_
#define SERVICES_ACCESSIBILITY_ACCESSIBILITY_SERVICE_CROS_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {

class AutomationImpl;
class AssistiveTechnologyControllerImpl;

// Implementation of the Accessibility Service for Chrome OS.
class AccessibilityServiceCros : public mojom::AccessibilityService {
 public:
  explicit AccessibilityServiceCros(
      mojo::PendingReceiver<mojom::AccessibilityService> receiver);
  ~AccessibilityServiceCros() override;
  AccessibilityServiceCros(const AccessibilityServiceCros&) = delete;
  AccessibilityServiceCros& operator=(const AccessibilityServiceCros&) = delete;

 private:
  friend class AccessibilityServiceCrosTest;

  // mojom::AccessibilityService:
  void BindAutomation(
      mojo::PendingRemote<mojom::AutomationClient> accessibility_client_remote,
      mojo::PendingReceiver<mojom::Automation> automation_receiver) override;
  void BindAssistiveTechnologyController(
      mojo::PendingReceiver<mojom::AssistiveTechnologyController>
          at_controller_receiver,
      const std::vector<mojom::AssistiveTechnologyType>& enabled_features)
      override;

  std::unique_ptr<AssistiveTechnologyControllerImpl> at_controller_;

  std::unique_ptr<AutomationImpl> automation_;

  mojo::Receiver<mojom::AccessibilityService> receiver_;

  base::WeakPtrFactory<AccessibilityServiceCros> weak_ptr_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_ACCESSIBILITY_SERVICE_CROS_H_
