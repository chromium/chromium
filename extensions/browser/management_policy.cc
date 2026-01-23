// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/management_policy.h"

#include "base/barrier_callback.h"
#include "base/logging.h"

namespace extensions {

namespace {

void GetExtensionNameAndId(const Extension* extension,
                           std::string* name,
                           std::string* id) {
  // The extension may be NULL in testing.
  *id = extension ? extension->id() : "[test]";
  *name = extension ? extension->name() : "test";
}

// Context object for UserMayInstall, instead of passing a lot of arguments
// around.
struct UserMayInstallContext {
  scoped_refptr<const Extension> extension;
#if DCHECK_IS_ON() && !defined(NDEBUG)
  // Extra context for logging in debug builds.
  const char* debug_operation_name;
  std::vector<std::string> provider_names;
#endif  // DCHECK_IS_ON() && !defined(NDEBUG)
};

// Callback for the BarrierCallback in UserMayInstall().
void OnUserMayInstallDone(
    UserMayInstallContext context,
    base::OnceCallback<void(ManagementPolicy::Decision)> callback,
    std::vector<ManagementPolicy::Decision> decisions) {
  const bool normal_result = true;
  for (size_t i = 0; i < decisions.size(); ++i) {
    const ManagementPolicy::Decision& decision = decisions[i];
    if (decision.allowed != normal_result) {
#if DCHECK_IS_ON() && !defined(NDEBUG)
      std::string extension_name;
      std::string extension_id;
      GetExtensionNameAndId(context.extension.get(), &extension_name,
                            &extension_id);
      DCHECK(i < context.provider_names.size());
      DVLOG(1) << context.debug_operation_name << " of extension "
               << extension_name << " (" << extension_id << ")"
               << " prohibited by " << context.provider_names[i];
#endif  // DCHECK_IS_ON() && !defined(NDEBUG)
      std::move(callback).Run({!normal_result, std::move(decision.error)});
      return;
    }
  }
  std::move(callback).Run({normal_result, std::u16string()});
}

}  // namespace

ManagementPolicy::ManagementPolicy() {
}

ManagementPolicy::~ManagementPolicy() {
}

bool ManagementPolicy::Provider::UserMayLoad(const Extension* extension,
                                             std::u16string* error) const {
  return true;
}

void ManagementPolicy::Provider::UserMayInstall(
    scoped_refptr<const Extension> extension,
    base::OnceCallback<void(Decision)> callback) const {
  std::u16string error;
  bool may_load = UserMayLoad(extension.get(), &error);
  std::move(callback).Run({may_load, std::move(error)});
}

bool ManagementPolicy::Provider::UserMayModifySettings(
    const Extension* extension,
    std::u16string* error) const {
  return true;
}

bool ManagementPolicy::Provider::ExtensionMayModifySettings(
    const Extension* source_extension,
    const Extension* extension,
    std::u16string* error) const {
  return true;
}

bool ManagementPolicy::Provider::MustRemainEnabled(
    const Extension* extension,
    std::u16string* error) const {
  return false;
}

bool ManagementPolicy::Provider::MustRemainDisabled(
    const Extension* extension,
    disable_reason::DisableReason* reason) const {
  return false;
}

bool ManagementPolicy::Provider::MustRemainInstalled(
    const Extension* extension,
    std::u16string* error) const {
  return false;
}

bool ManagementPolicy::Provider::ShouldForceUninstall(
    const Extension* extension,
    std::u16string* error) const {
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

bool ManagementPolicy::UserMayLoad(const Extension* extension) const {
  return ApplyToProviderList(&Provider::UserMayLoad, "Installation", true,
                             extension, /*error=*/nullptr);
}

void ManagementPolicy::UserMayInstall(
    scoped_refptr<const Extension> extension,
    base::OnceCallback<void(Decision)> callback) const {
  UserMayInstallContext context;
  context.extension = extension;
#if DCHECK_IS_ON() && !defined(NDEBUG)
  context.debug_operation_name = "Installation";
  std::ranges::transform(providers_, std::back_inserter(context.provider_names),
                         &Provider::GetDebugPolicyProviderName);
#endif  // DCHECK_IS_ON() && !defined(NDEBUG)

  auto barrier = base::BarrierCallback<Decision>(
      providers_.size(),
      base::BindOnce(&OnUserMayInstallDone, std::move(context),
                     std::move(callback)));
  for (const Provider* provider : providers_) {
    provider->UserMayInstall(extension, barrier);
  }
}

bool ManagementPolicy::UserMayModifySettings(const Extension* extension,
                                             std::u16string* error) const {
  return ApplyToProviderList(
      &Provider::UserMayModifySettings, "Modification", true, extension, error);
}

bool ManagementPolicy::ExtensionMayModifySettings(
    const Extension* source_extension,
    const Extension* extension,
    std::u16string* error) const {
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
                                         std::u16string* error) const {
  return ApplyToProviderList(
      &Provider::MustRemainEnabled, "Disabling", false, extension, error);
}

bool ManagementPolicy::MustRemainDisabled(
    const Extension* extension,
    disable_reason::DisableReason* reason) const {
  if (!UserMayLoad(extension)) {
    if (reason) {
      *reason = disable_reason::DISABLE_BLOCKED_BY_POLICY;
    }
    return true;
  }

  for (const auto& provider : providers_) {
    if (provider->MustRemainDisabled(extension, reason)) {
      return true;
    }
  }

  return false;
}

bool ManagementPolicy::MustRemainInstalled(const Extension* extension,
                                           std::u16string* error) const {
  return ApplyToProviderList(
      &Provider::MustRemainInstalled, "Removing", false, extension, error);
}

bool ManagementPolicy::ShouldForceUninstall(const Extension* extension,
                                            std::u16string* error) const {
  return ApplyToProviderList(&Provider::ShouldForceUninstall, "Uninstalling",
                             false, extension, error);
}

bool ManagementPolicy::ShouldRepairIfCorrupted(const Extension* extension) {
  return MustRemainEnabled(extension, nullptr) ||
         MustRemainInstalled(extension, nullptr);
}

bool ManagementPolicy::HasEnterpriseForcedAccess(
    const extensions::Extension& extension) const {
  return !UserMayModifySettings(&extension, nullptr) ||
         MustRemainInstalled(&extension, nullptr);
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
                                           std::u16string* error) const {
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
