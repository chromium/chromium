// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mock_extension_system.h"

#include "components/value_store/value_store_factory.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

namespace extensions {

MockExtensionSystem::MockExtensionSystem(content::BrowserContext* context)
    : browser_context_(context) {
}

MockExtensionSystem::~MockExtensionSystem() = default;

void MockExtensionSystem::SetReady() {
  ready_.Signal();
}

void MockExtensionSystem::InitForRegularProfile(bool extensions_enabled) {}

ExtensionService* MockExtensionSystem::extension_service() {
  return nullptr;
}

ManagementPolicy* MockExtensionSystem::management_policy() {
  return nullptr;
}

ServiceWorkerManager* MockExtensionSystem::service_worker_manager() {
  return nullptr;
}

UserScriptManager* MockExtensionSystem::user_script_manager() {
  return nullptr;
}

StateStore* MockExtensionSystem::state_store() {
  return nullptr;
}

StateStore* MockExtensionSystem::rules_store() {
  return nullptr;
}

StateStore* MockExtensionSystem::dynamic_user_scripts_store() {
  return nullptr;
}

scoped_refptr<value_store::ValueStoreFactory>
MockExtensionSystem::store_factory() {
  return nullptr;
}

QuotaService* MockExtensionSystem::quota_service() {
  return nullptr;
}

AppSorting* MockExtensionSystem::app_sorting() {
  return nullptr;
}

const base::OneShotEvent& MockExtensionSystem::ready() const {
  return ready_;
}

bool MockExtensionSystem::is_ready() const {
  return ready_.is_signaled();
}

ContentVerifier* MockExtensionSystem::content_verifier() {
  return nullptr;
}

std::unique_ptr<ExtensionSet> MockExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return nullptr;
}

void MockExtensionSystem::InstallUpdate(
    const ExtensionId& extension_id,
    const std::string& public_key,
    const base::FilePath& temp_dir,
    bool install_immediately,
    InstallUpdateCallback install_update_callback) {
  NOTREACHED_IN_MIGRATION();
}

void MockExtensionSystem::PerformActionBasedOnOmahaAttributes(
    const ExtensionId& extension_id,
    const base::Value::Dict& attributes) {}

bool MockExtensionSystem::FinishDelayedInstallationIfReady(
    const ExtensionId& extension_id,
    bool install_immediately) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace extensions
