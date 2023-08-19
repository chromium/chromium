// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_OS_ACCESSIBILITY_SERVICE_H_
#define SERVICES_ACCESSIBILITY_OS_ACCESSIBILITY_SERVICE_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {
class AssistiveTechnologyControllerImpl;

// Implementation of the Accessibility Service which includes assistive
// technology features that run in the service. Used by Chrome OS and Fuchsia.
class OSAccessibilityService : public mojom::AccessibilityService {
 public:
  explicit OSAccessibilityService(
      mojo::PendingReceiver<mojom::AccessibilityService> receiver);
  ~OSAccessibilityService() override;
  OSAccessibilityService(const OSAccessibilityService&) = delete;
  OSAccessibilityService& operator=(const OSAccessibilityService&) = delete;

 private:
  friend class OSAccessibilityServiceTest;
  friend class AssistiveTechnologyControllerTest;
  friend class AtpJSApiTest;

  // mojom::AccessibilityService:
  void BindAccessibilityServiceClient(
      mojo::PendingRemote<mojom::AccessibilityServiceClient>
          accessibility_client_remote) override;
  void BindAssistiveTechnologyController(
      mojo::PendingReceiver<mojom::AssistiveTechnologyController>
          at_controller_receiver,
      const std::vector<mojom::AssistiveTechnologyType>& enabled_features)
      override;

  void ConnectDevToolsAgent(
      ::mojo::PendingAssociatedReceiver<::blink::mojom::DevToolsAgent> agent,
      mojom::AssistiveTechnologyType type) override;

  std::unique_ptr<AssistiveTechnologyControllerImpl> at_controller_;

  mojo::Receiver<mojom::AccessibilityService> receiver_;

  base::WeakPtrFactory<OSAccessibilityService> weak_ptr_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_OS_ACCESSIBILITY_SERVICE_H_
