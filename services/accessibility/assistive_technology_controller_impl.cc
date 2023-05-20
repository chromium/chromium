// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/assistive_technology_controller_impl.h"

#include <memory>

#include "services/accessibility/automation_impl.h"
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
}

void AssistiveTechnologyControllerImpl::BindAutomation(
    mojo::PendingRemote<mojom::Automation> automation,
    mojo::PendingReceiver<mojom::AutomationClient> automation_client) {
  accessibility_service_client_remote_->BindAutomation(
      std::move(automation), std::move(automation_client));
}

void AssistiveTechnologyControllerImpl::BindTts(
    mojo::PendingReceiver<mojom::Tts> tts_receiver) {
  accessibility_service_client_remote_->BindTts(std::move(tts_receiver));
}

void AssistiveTechnologyControllerImpl::EnableAssistiveTechnology(
    const std::vector<mojom::AssistiveTechnologyType>& enabled_features) {
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i <= static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    bool enabled = std::find(enabled_features.begin(), enabled_features.end(),
                             type) != enabled_features.end();
    auto it = enabled_ATs_.find(type);
    if (enabled && it == enabled_ATs_.end()) {
      enabled_ATs_[type] = GetOrMakeV8Manager(type);
    } else if (!enabled && it != enabled_ATs_.end()) {
      enabled_ATs_.erase(type);
    }
  }
}

bool AssistiveTechnologyControllerImpl::IsFeatureEnabled(
    mojom::AssistiveTechnologyType type) const {
  return enabled_ATs_.find(type) != enabled_ATs_.end();
}

void AssistiveTechnologyControllerImpl::RunScriptForTest(
    mojom::AssistiveTechnologyType type,
    const std::string& script,
    base::OnceClosure on_complete) {
  enabled_ATs_[type]->ExecuteScript(script, std::move(on_complete));
}

void AssistiveTechnologyControllerImpl::SetTestInterface(
    mojom::AssistiveTechnologyType type,
    std::unique_ptr<InterfaceBinder> test_interface) {
  enabled_ATs_[type]->SetTestMojoInterface(std::move(test_interface));
}

scoped_refptr<V8Manager> AssistiveTechnologyControllerImpl::GetOrMakeV8Manager(
    mojom::AssistiveTechnologyType type) {
  // For the first one we can ask it to initialize v8.
  if (!v8_initialized_) {
    BindingsIsolateHolder::InitializeV8();
    v8_initialized_ = true;
  }

  scoped_refptr<V8Manager> v8_manager = V8Manager::Create();

  // Install bindings on the global context depending on the type.
  // For example, some types may need TTS and some may not. All need Automation.
  v8_manager->InstallAutomation(weak_ptr_factory_.GetWeakPtr());
  if (type == mojom::AssistiveTechnologyType::kChromeVox ||
      type == mojom::AssistiveTechnologyType::kSelectToSpeak) {
    v8_manager->InstallTts(weak_ptr_factory_.GetWeakPtr());
  }
  // TODO(crbug.com/1355633): Install other bindings based on the type
  // once they are implemented.

  // After installing all bindings, initialize.
  v8_manager->AddV8Bindings();
  return v8_manager;
}

}  // namespace ax
