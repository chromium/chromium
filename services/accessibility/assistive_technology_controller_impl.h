// Copyright 2022 The Chromium Authors
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
#include "services/accessibility/public/mojom/autoclick.mojom-forward.h"
#include "services/accessibility/public/mojom/file_loader.mojom.h"
#include "services/accessibility/public/mojom/user_input.mojom-forward.h"
#include "services/accessibility/public/mojom/user_interface.mojom-forward.h"

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

  // Called by a bindings class owned by a V8 instance to request binding of
  // mojo interfaces in the OS.
  // mojom::AccessibilityServiceClient:
  void BindAutomation(
      mojo::PendingAssociatedRemote<mojom::Automation> automation) override;
  void BindAutomationClient(mojo::PendingReceiver<mojom::AutomationClient>
                                automation_client) override;
  void BindAutoclickClient(
      mojo::PendingReceiver<mojom::AutoclickClient> autoclick_client) override;
  void BindSpeechRecognition(
      mojo::PendingReceiver<mojom::SpeechRecognition> sr_receiver) override;
  void BindTts(mojo::PendingReceiver<mojom::Tts> tts_receiver) override;
  void BindUserInput(
      mojo::PendingReceiver<mojom::UserInput> user_input_receiver) override;
  void BindUserInterface(mojo::PendingReceiver<mojom::UserInterface>
                             user_interface_receiver) override;
  void BindAccessibilityFileLoader(
      mojo::PendingReceiver<ax::mojom::AccessibilityFileLoader>
          file_loader_receiver) override;

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

  V8Manager& GetOrCreateV8Manager(mojom::AssistiveTechnologyType type);

 private:
  void CreateV8ManagerForTypeIfNoneExists(mojom::AssistiveTechnologyType type);

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

  // Interface used to request loading of files by the accessibility service.
  // This remote is shared between the V8Managers that are instantiated, but can
  // be moved to be owned by the manager once only one instance of the manager
  // exists.
  mojo::Remote<mojom::AccessibilityFileLoader> file_loader_remote_;
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_ASSISTIVE_TECHNOLOGY_CONTROLLER_IMPL_H_
