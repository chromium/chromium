// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/management_policy.h"

namespace extensions {

namespace {

void GetExtensionNameAndId(const Extension* extension,
                           std::string* name,
                           std::string* id) {
  // The extension may be NULL in testing.
  *id = extension ? extension->id() : "[test]";
  *name = extension ? extension->name() : "test";
}

}  // namespace

ManagementPolicy::ManagementPolicy() {
}

ManagementPolicy::~ManagementPolicy() {
}

bool ManagementPolicy::Provider::UserMayLoad(const Extension* extension,
                                             base::string16* error) const {
  return true;
}

bool ManagementPolicy::Provider::UserMayInstall(const Extension* extension,
                                                base::string16* error) const {
  return UserMayLoad(extension, error);
}

bool ManagementPolicy::Provider::UserMayModifySettings(
    const Extension* extension, base::string16* error) const {
  return true;
}

bool ManagementPolicy::Provider::ExtensionMayModifySettings(
    const Extension* source_extension,
    const Extension* extension,
    base::string16* error) const {
  return true;
}

bool ManagementPolicy::Provider::MustRemainEnabled(const Extension* extension,
                                                   base::string16* error)
    const {
  return false;
}

bool ManagementPolicy::Provider::MustRemainDisabled(
    const Extension* extension,
    disable_reason::DisableReason* reason,
    base::string16* error) const {
  return false;
}

bool ManagementPolicy::Provider::MustRemainInstalled(
    const Extension* extension,
    base::string16* error) const {
  return false;
}

bool ManagementPolicy::Provider::ShouldForceUninstall(
    const Extension* extension,
    base::string16* error) const {
  return false;
}

void ManagementPolicy::RegisterProvider(Provider* provider) {
  providers_.insert(provider);
}

void ManagementPolicy::UnregisterProvider(Provider* provider) {
  providers_.erase(provider);
}

void ManagementPolicy::RegisterProviders(
    const std::vector<std::unique_ptr<Provider>>& providers) {
  for (const std::unique_ptr<Provider>& provider : providers)
    providers_.insert(provider.get());
}

bool ManagementPolicy::UserMayLoad(const Extension* extension,
                                   base::string16* error) const {
  return ApplyToProviderList(
      &Provider::UserMayLoad, "Installation", true, extension, error);
}

bool ManagementPolicy::UserMayInstall(const Extension* extension,
                                      base::string16* error) const {
  return ApplyToProviderList(&Provider::UserMayInstall, "Installation", true,
                             extension, error);
}

bool ManagementPolicy::UserMayModifySettings(const Extension* extension,
                                             base::string16* error) const {
  return ApplyToProviderList(
      &Provider::UserMayModifySettings, "Modification", true, extension, error);
}

bool ManagementPolicy::ExtensionMayModifySettings(
    const Extension* source_extension,
    const Extension* extension,
    base::string16* error) const {
  for (const Provider* provider : providers_) {
    if (!provider->ExtensionMayModifySettings(source_extension, extension,
                                              error)) {
      std::string id;
      std::string name;
      GetExtensionNameAndId(extension, &name, &id);
      DVLOG(1) << "Modification of extension " << name << " (" << id << ")"
               << " prohibited by " << provider->GetDebugPolicyProviderName();
      return false;
    }
  }
  return true;
}

bool ManagementPolicy::MustRemainEnabled(const Extension* extension,
                                         base::string16* error) const {
  return ApplyToProviderList(
      &Provider::MustRemainEnabled, "Disabling", false, extension, error);
}

bool ManagementPolicy::MustRemainDisabled(const Extension* extension,
                                          disable_reason::DisableReason* reason,
                                          base::string16* error) const {
  if (!UserMayLoad(extension, error)) {
    if (reason)
      *reason = disable_reason::DISABLE_BLOCKED_BY_POLICY;
    return true;
  }

  for (auto it = providers_.cbegin(); it != providers_.cend(); ++it)
    if ((*it)->MustRemainDisabled(extension, reason, error))
      return true;

  return false;
}

bool ManagementPolicy::MustRemainInstalled(const Extension* extension,
                                           base::string16* error) const {
  return ApplyToProviderList(
      &Provider::MustRemainInstalled, "Removing", false, extension, error);
}

bool ManagementPolicy::ShouldForceUninstall(const Extension* extension,
                                            base::string16* error) const {
  return ApplyToProviderList(&Provider::ShouldForceUninstall, "Uninstalling",
                             false, extension, error);
}

bool ManagementPolicy::ShouldRepairIfCorrupted(const Extension* extension) {
  return MustRemainEnabled(extension, nullptr) ||
         MustRemainInstalled(extension, nullptr);
}

void ManagementPolicy::UnregisterAllProviders() {
  providers_.clear();
}

int ManagementPolicy::GetNumProviders() const {
  return providers_.size();
}

bool ManagementPolicy::ApplyToProviderList(ProviderFunction function,
                                           const char* debug_operation_name,
                                           bool normal_result,
                                           const Extension* extension,
                                           base::string16* error) const {
  for (const Provider* provider : providers_) {
    bool result = (provider->*function)(extension, error);
    if (result != normal_result) {
      std::string id;
      std::string name;
      GetExtensionNameAndId(extension, &name, &id);
      DVLOG(1) << debug_operation_name << " of extension " << name
               << " (" << id << ")"
               << " prohibited by " << provider->GetDebugPolicyProviderName();
      return !normal_result;
    }
  }
  return normal_result;
}

}  // namespace extensions
