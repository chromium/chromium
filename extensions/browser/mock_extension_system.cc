// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mock_extension_system.h"

#include "extensions/browser/value_store/value_store_factory.h"
#include "extensions/common/extension_set.h"

namespace extensions {

MockExtensionSystem::MockExtensionSystem(content::BrowserContext* context)
    : browser_context_(context) {
}

MockExtensionSystem::~MockExtensionSystem() {
}

void MockExtensionSystem::InitForRegularProfile(bool extensions_enabled) {}

ExtensionService* MockExtensionSystem::extension_service() {
  return nullptr;
}

RuntimeData* MockExtensionSystem::runtime_data() {
  return nullptr;
}

ManagementPolicy* MockExtensionSystem::management_policy() {
  return nullptr;
}

ServiceWorkerManager* MockExtensionSystem::service_worker_manager() {
  return nullptr;
}

SharedUserScriptMaster* MockExtensionSystem::shared_user_script_master() {
  return nullptr;
}

StateStore* MockExtensionSystem::state_store() {
  return nullptr;
}

StateStore* MockExtensionSystem::rules_store() {
  return nullptr;
}

scoped_refptr<ValueStoreFactory> MockExtensionSystem::store_factory() {
  return nullptr;
}

InfoMap* MockExtensionSystem::info_map() {
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

ContentVerifier* MockExtensionSystem::content_verifier() {
  return nullptr;
}

std::unique_ptr<ExtensionSet> MockExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return std::unique_ptr<ExtensionSet>();
}

void MockExtensionSystem::InstallUpdate(
    const std::string& extension_id,
    const std::string& public_key,
    const base::FilePath& temp_dir,
    bool install_immediately,
    InstallUpdateCallback install_update_callback) {
  NOTREACHED();
}

bool MockExtensionSystem::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  NOTREACHED();
  return false;
}

}  // namespace extensions
