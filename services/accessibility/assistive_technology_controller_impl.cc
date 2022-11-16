// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/assistive_technology_controller_impl.h"

#include <memory>

#include "services/accessibility/automation_impl.h"
#include "services/accessibility/features/v8_manager.h"

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

void AssistiveTechnologyControllerImpl::BindAutomation(
    mojo::PendingRemote<mojom::Automation> automation,
    mojo::PendingReceiver<mojom::AutomationClient> automation_client) {
  if (automation_bound_closure_for_test_) {
    std::move(automation_bound_closure_for_test_).Run();
  }
  // TODO(crbug.com/1355633): Bind to Automation in the embedding OS
  // after updating the mojom. See go/chromeos-atp-v8-design.
}

void AssistiveTechnologyControllerImpl::EnableAssistiveTechnology(
    mojom::AssistiveTechnologyType type,
    bool enabled) {
  auto it = enabled_ATs_.find(type);
  if (enabled && it == enabled_ATs_.end()) {
    enabled_ATs_[type] = GetOrMakeV8Manager(type);
  } else if (!enabled && it != enabled_ATs_.end()) {
    enabled_ATs_.erase(type);
  }
}

bool AssistiveTechnologyControllerImpl::IsFeatureEnabled(
    mojom::AssistiveTechnologyType type) const {
  return enabled_ATs_.find(type) != enabled_ATs_.end();
}

void AssistiveTechnologyControllerImpl::SetAutomationBoundClosureForTest(
    base::OnceClosure closure) {
  automation_bound_closure_for_test_ = std::move(closure);
}

void AssistiveTechnologyControllerImpl::RunScriptForTest(
    mojom::AssistiveTechnologyType type,
    const std::string& script,
    base::OnceClosure on_complete) {
  enabled_ATs_[type]->ExecuteScript(script, std::move(on_complete));
}

scoped_refptr<V8Manager> AssistiveTechnologyControllerImpl::GetOrMakeV8Manager(
    mojom::AssistiveTechnologyType type) {
  // For the first one we can ask it to initialize v8.
  if (!v8_initialized_) {
    V8Manager::InitializeV8();
    v8_initialized_ = true;
  }

  scoped_refptr<V8Manager> v8_manager = V8Manager::Create();

  // Install bindings on the global context depending on the type.
  // For example, some types may need TTS and some may not. All need Automation.
  v8_manager->InstallAutomation(weak_ptr_factory_.GetWeakPtr());
  // TODO(crbug.com/1355633): Install other bindings based on the type
  // once they are implemented.

  // After installing all bindings, initialize.
  v8_manager->AddV8Bindings();
  return v8_manager;
}

}  // namespace ax
