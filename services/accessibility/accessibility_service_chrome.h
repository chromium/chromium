// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ACCESSIBILITY_SERVICE_CHROME_H_
#define SERVICES_ACCESSIBILITY_ACCESSIBILITY_SERVICE_CHROME_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {
class AutomationImpl;

// Implementation of the Accessibility Service for Chrome.
class AccessibilityServiceChrome : public mojom::AccessibilityService {
 public:
  explicit AccessibilityServiceChrome(
      mojo::PendingReceiver<mojom::AccessibilityService> receiver);
  ~AccessibilityServiceChrome() override;
  AccessibilityServiceChrome(const AccessibilityServiceChrome&) = delete;
  AccessibilityServiceChrome& operator=(const AccessibilityServiceChrome&) =
      delete;

 private:
  // mojom::AccessibilityService:
  void BindAutomation(
      mojo::PendingRemote<mojom::AutomationClient> accessibility_client_remote,
      mojo::PendingReceiver<mojom::Automation> automation_receiver) override;

  std::unique_ptr<AutomationImpl> automation_;

  mojo::Receiver<mojom::AccessibilityService> receiver_;

  base::WeakPtrFactory<AccessibilityServiceChrome> weak_ptr_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_ACCESSIBILITY_SERVICE_CHROME_H_
