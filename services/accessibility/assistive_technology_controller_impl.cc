// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/assistive_technology_controller_impl.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"
#include "services/accessibility/automation_impl.h"
#include "services/accessibility/features/interface_binder.h"
#include "services/accessibility/features/v8_manager.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {

AssistiveTechnologyControllerImpl::AssistiveTechnologyControllerImpl() =
    default;

AssistiveTechnologyControllerImpl::~AssistiveTechnologyControllerImpl() =
    default;

void AssistiveTechnologyControllerImpl::Bind(
    mojo::PendingReceiver<mojom::AssistiveTechnologyController>
        at_controller_receiver) {
  DCHECK(!at_controller_receiver_.is_bound());
  at_controller_receiver_.Bind(std::move(at_controller_receiver));
}

void AssistiveTechnologyControllerImpl::BindAccessibilityServiceClient(
    mojo::PendingRemote<mojom::AccessibilityServiceClient>
        accessibility_client_remote) {
  DCHECK(!accessibility_service_client_remote_.is_bound());
  accessibility_service_client_remote_.Bind(
      std::move(accessibility_client_remote));

  // Once we have access to the AccessibilityServiceClient, initialize file
  // loading capabilities.
  accessibility_service_client_remote_->BindAccessibilityFileLoader(
      file_loader_remote_.BindNewPipeAndPassReceiver());
}

void AssistiveTechnologyControllerImpl::BindAutomation(
    mojo::PendingAssociatedRemote<mojom::Automation> automation) {
  accessibility_service_client_remote_->BindAutomation(std::move(automation));
}

void AssistiveTechnologyControllerImpl::BindAutomationClient(
    mojo::PendingReceiver<mojom::AutomationClient> automation_client) {
  accessibility_service_client_remote_->BindAutomationClient(
      std::move(automation_client));
}

void AssistiveTechnologyControllerImpl::BindAutoclickClient(
    mojo::PendingReceiver<mojom::AutoclickClient> autoclick_client) {
  accessibility_service_client_remote_->BindAutoclickClient(
      std::move(autoclick_client));
}

void AssistiveTechnologyControllerImpl::BindSpeechRecognition(
    mojo::PendingReceiver<mojom::SpeechRecognition> sr_receiver) {
  accessibility_service_client_remote_->BindSpeechRecognition(
      std::move(sr_receiver));
}

void AssistiveTechnologyControllerImpl::BindTts(
    mojo::PendingReceiver<mojom::Tts> tts_receiver) {
  accessibility_service_client_remote_->BindTts(std::move(tts_receiver));
}

void AssistiveTechnologyControllerImpl::BindUserInput(
    mojo::PendingReceiver<mojom::UserInput> user_input_receiver) {
  accessibility_service_client_remote_->BindUserInput(
      std::move(user_input_receiver));
}

void AssistiveTechnologyControllerImpl::BindUserInterface(
    mojo::PendingReceiver<mojom::UserInterface> user_interface_receiver) {
  accessibility_service_client_remote_->BindUserInterface(
      std::move(user_interface_receiver));
}

void AssistiveTechnologyControllerImpl::BindAccessibilityFileLoader(
    mojo::PendingReceiver<ax::mojom::AccessibilityFileLoader>
        file_loader_receiver) {
  NOTREACHED_IN_MIGRATION();
}

void AssistiveTechnologyControllerImpl::EnableAssistiveTechnology(
    const std::vector<mojom::AssistiveTechnologyType>& enabled_features) {
  for (auto i = base::to_underlying(mojom::AssistiveTechnologyType::kMinValue);
       i <= base::to_underlying(mojom::AssistiveTechnologyType::kMaxValue);
       i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    bool enabled = base::Contains(enabled_features, type);
    auto it = enabled_ATs_.find(type);
    if (enabled && it == enabled_ATs_.end()) {
      // TODO(b/293348920): Re-use an existing V8Manager and install additional
      // bindings there rather than always making a new one when sharing
      // V8Managers across different AT types.
      CreateV8ManagerForTypeIfNoneExists(type);
    } else if (!enabled && it != enabled_ATs_.end()) {
      enabled_ATs_.erase(type);
    }
  }
}

bool AssistiveTechnologyControllerImpl::IsFeatureEnabled(
    mojom::AssistiveTechnologyType type) const {
  return base::Contains(enabled_ATs_, type);
}

void AssistiveTechnologyControllerImpl::RunScriptForTest(
    mojom::AssistiveTechnologyType type,
    const std::string& script,
    base::OnceClosure on_complete) {
  GetOrCreateV8Manager(type).RunScriptForTest(  // IN-TEST
      script, std::move(on_complete));
}

void AssistiveTechnologyControllerImpl::AddInterfaceForTest(
    mojom::AssistiveTechnologyType type,
    std::unique_ptr<InterfaceBinder> test_interface) {
  GetOrCreateV8Manager(type).AddInterfaceForTest(  // IN-TEST
      std::move(test_interface));
}

V8Manager& AssistiveTechnologyControllerImpl::GetOrCreateV8Manager(
    mojom::AssistiveTechnologyType type) {
  CreateV8ManagerForTypeIfNoneExists(type);
  return enabled_ATs_[type];
}

void AssistiveTechnologyControllerImpl::CreateV8ManagerForTypeIfNoneExists(
    mojom::AssistiveTechnologyType type) {
  // Do nothing if the manager already exists.
  if (auto it = enabled_ATs_.find(type); it != enabled_ATs_.end()) {
    return;
  }

  // For the first one we can ask it to initialize v8.
  if (!v8_initialized_) {
    BindingsIsolateHolder::InitializeV8();
    v8_initialized_ = true;
  }

  V8Manager& manager = enabled_ATs_[type];

  // Install bindings on the global context depending on the type.
  // For example, some types may need TTS and some may not. All need Automation
  // and file loading.
  // TODO(b/262637071): Create a easy way to map AT types to APIs needed instead
  // of these large if statements.
  mojo::PendingAssociatedReceiver<mojom::Automation> automation;
  BindAutomation(automation.InitWithNewEndpointAndPassRemote());
  manager.ConfigureAutomation(this, std::move(automation));
  manager.ConfigureFileLoader(&file_loader_remote_);
  if (type == mojom::AssistiveTechnologyType::kChromeVox ||
      type == mojom::AssistiveTechnologyType::kDictation) {
    manager.ConfigureOSState();
  }
  if (type == mojom::AssistiveTechnologyType::kChromeVox ||
      type == mojom::AssistiveTechnologyType::kSelectToSpeak) {
    manager.ConfigureTts(this);
  }
  if (type == mojom::AssistiveTechnologyType::kChromeVox ||
      type == mojom::AssistiveTechnologyType::kSelectToSpeak ||
      type == mojom::AssistiveTechnologyType::kAutoClick ||
      type == mojom::AssistiveTechnologyType::kSwitchAccess) {
    manager.ConfigureUserInterface(this);
  }
  if (type == mojom::AssistiveTechnologyType::kDictation) {
    manager.ConfigureSpeechRecognition(this);
  }
  if (type == mojom::AssistiveTechnologyType::kAutoClick) {
    manager.ConfigureAutoclick(this);
  }
  if (type == mojom::AssistiveTechnologyType::kAutoClick ||
      type == mojom::AssistiveTechnologyType::kChromeVox ||
      type == mojom::AssistiveTechnologyType::kDictation ||
      type == mojom::AssistiveTechnologyType::kMagnifier ||
      type == mojom::AssistiveTechnologyType::kSelectToSpeak ||
      type == mojom::AssistiveTechnologyType::kSwitchAccess) {
    manager.ConfigureUserInput(this);
  }
  // TODO(b/262637071): Configure other bindings based on the type
  // once they are implemented.

  // After configuring all bindings, initialize.
  manager.FinishContextSetUp();
}

}  // namespace ax
