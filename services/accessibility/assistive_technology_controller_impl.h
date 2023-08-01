// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ASSISTIVE_TECHNOLOGY_CONTROLLER_IMPL_H_
#define SERVICES_ACCESSIBILITY_ASSISTIVE_TECHNOLOGY_CONTROLLER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {

class InterfaceBinder;
class V8Manager;

// Implementation of the assistive technology controller interface
// for Chrome OS. This tracks which features are enabled and will
// load/unload feature implementations from V8 as needed.
class AssistiveTechnologyControllerImpl
    : public mojom::AssistiveTechnologyController,
      public mojom::AccessibilityServiceClient {
 public:
  AssistiveTechnologyControllerImpl();
  ~AssistiveTechnologyControllerImpl() override;
  AssistiveTechnologyControllerImpl(const AssistiveTechnologyControllerImpl&) =
      delete;
  AssistiveTechnologyControllerImpl& operator=(
      const AssistiveTechnologyControllerImpl&) = delete;

  // Called by the AccessibilityServiceCros.
  void Bind(mojo::PendingReceiver<mojom::AssistiveTechnologyController>
                at_controller_receiver);
  void BindAccessibilityServiceClient(
      mojo::PendingRemote<mojom::AccessibilityServiceClient>
          accessibility_client_remote);

  // Called by AutomationInternalBindings owned by a V8 instance
  // to request binding of mojo interfaces in the OS.
  // mojom::AccessibilityServiceClient:
  void BindTts(mojo::PendingReceiver<mojom::Tts> tts_receiver) override;
  void BindAutomation(
      mojo::PendingAssociatedRemote<mojom::Automation> automation,
      mojo::PendingReceiver<mojom::AutomationClient> automation_client)
      override;

  // mojom::AssistiveTechnologyController:
  void EnableAssistiveTechnology(
      const std::vector<mojom::AssistiveTechnologyType>& enabled_features)
      override;

  bool IsFeatureEnabled(mojom::AssistiveTechnologyType type) const;

  // Methods for testing.
  void RunScriptForTest(mojom::AssistiveTechnologyType type,
                        const std::string& script,
                        base::OnceClosure on_complete);
  void AddInterfaceForTest(mojom::AssistiveTechnologyType type,
                           std::unique_ptr<InterfaceBinder> test_interface);

 private:
  void CreateV8ManagerForType(mojom::AssistiveTechnologyType type);

  std::map<mojom::AssistiveTechnologyType, V8Manager> enabled_ATs_;

  // Whether V8 has been initialized once. Allows us to only
  // initialize V8 for the service one time. Assumes this class has the same
  // lifetime as the service (as it's constructed and owned by the
  // AccessibilityServiceCros).
  bool v8_initialized_ = false;

  // This class is a receiver for mojom::AssistiveTechnologyController.
  mojo::Receiver<mojom::AssistiveTechnologyController> at_controller_receiver_{
      this};

  // The remote to the Accessibility Service Client in the OS.
  mojo::Remote<mojom::AccessibilityServiceClient>
      accessibility_service_client_remote_;
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_ASSISTIVE_TECHNOLOGY_CONTROLLER_IMPL_H_
