// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registry.h"

#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

ExtensionRegistry::ExtensionRegistry(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}
ExtensionRegistry::~ExtensionRegistry() = default;

// static
ExtensionRegistry* ExtensionRegistry::Get(content::BrowserContext* context) {
  return ExtensionRegistryFactory::GetForBrowserContext(context);
}

ExtensionSet ExtensionRegistry::GenerateInstalledExtensionsSet() const {
  return GenerateInstalledExtensionsSet(EVERYTHING);
}

ExtensionSet ExtensionRegistry::GenerateInstalledExtensionsSet(
    int include_mask) const {
  ExtensionSet installed_extensions;
  if (include_mask & IncludeFlag::ENABLED)
    installed_extensions.InsertAll(enabled_extensions_);
  if (include_mask & IncludeFlag::DISABLED)
    installed_extensions.InsertAll(disabled_extensions_);
  if (include_mask & IncludeFlag::TERMINATED)
    installed_extensions.InsertAll(terminated_extensions_);
  if (include_mask & IncludeFlag::BLOCKLISTED)
    installed_extensions.InsertAll(blocklisted_extensions_);
  if (include_mask & IncludeFlag::BLOCKED)
    installed_extensions.InsertAll(blocked_extensions_);
  return installed_extensions;
}

base::Version ExtensionRegistry::GetStoredVersion(const ExtensionId& id) const {
  int include_mask = ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
                     ExtensionRegistry::TERMINATED |
                     ExtensionRegistry::BLOCKLISTED |
                     ExtensionRegistry::BLOCKED;
  const Extension* registry_extension = GetExtensionById(id, include_mask);
  return registry_extension ? registry_extension->version() : base::Version();
}

void ExtensionRegistry::AddObserver(ExtensionRegistryObserver* observer) {
  observers_.AddObserver(observer);
}

void ExtensionRegistry::RemoveObserver(ExtensionRegistryObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionRegistry::TriggerOnLoaded(const Extension* extension) {
  CHECK(extension);
  DCHECK(enabled_extensions_.Contains(extension->id()));
  for (auto& observer : observers_)
    observer.OnExtensionLoaded(browser_context_, extension);
}

void ExtensionRegistry::TriggerOnReady(const Extension* extension) {
  CHECK(extension);
  DCHECK(enabled_extensions_.Contains(extension->id()));
  for (auto& observer : observers_)
    observer.OnExtensionReady(browser_context_, extension);
}

void ExtensionRegistry::TriggerOnUnloaded(const Extension* extension,
                                          UnloadedExtensionReason reason) {
  CHECK(extension);
  DCHECK(!enabled_extensions_.Contains(extension->id()));
  for (auto& observer : observers_)
    observer.OnExtensionUnloaded(browser_context_, extension, reason);
}

void ExtensionRegistry::TriggerOnWillBeInstalled(const Extension* extension,
                                                 bool is_update,
                                                 const std::string& old_name) {
  CHECK(extension);
  DCHECK_EQ(is_update,
            GenerateInstalledExtensionsSet().Contains(extension->id()));
  DCHECK_EQ(is_update, !old_name.empty());
  for (auto& observer : observers_)
    observer.OnExtensionWillBeInstalled(browser_context_, extension, is_update,
                                        old_name);
}

void ExtensionRegistry::TriggerOnInstalled(const Extension* extension,
                                           bool is_update) {
  CHECK(extension);
  DCHECK(GenerateInstalledExtensionsSet().Contains(extension->id()));
  for (auto& observer : observers_)
    observer.OnExtensionInstalled(browser_context_, extension, is_update);
}

void ExtensionRegistry::TriggerOnUninstalled(const Extension* extension,
                                             UninstallReason reason) {
  CHECK(extension);
  DCHECK(!GenerateInstalledExtensionsSet().Contains(extension->id()));
  for (auto& observer : observers_)
    observer.OnExtensionUninstalled(browser_context_, extension, reason);
}

void ExtensionRegistry::TriggerOnUninstallationDenied(
    const Extension* extension) {
  CHECK(extension);
  DCHECK(GenerateInstalledExtensionsSet().Contains(extension->id()));
  for (auto& observer : observers_)
    observer.OnExtensionUninstallationDenied(browser_context_, extension);
}

const Extension* ExtensionRegistry::GetExtensionById(const std::string& id,
                                                     int include_mask) const {
  std::string lowercase_id = base::ToLowerASCII(id);
  if (include_mask & ENABLED) {
    const Extension* extension = enabled_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & DISABLED) {
    const Extension* extension = disabled_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & TERMINATED) {
    const Extension* extension = terminated_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & BLOCKLISTED) {
    const Extension* extension = blocklisted_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & BLOCKED) {
    const Extension* extension = blocked_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  return nullptr;
}

const Extension* ExtensionRegistry::GetInstalledExtension(
    const std::string& id) const {
  return GetExtensionById(id, ExtensionRegistry::EVERYTHING);
}

bool ExtensionRegistry::AddEnabled(
    const scoped_refptr<const Extension>& extension) {
  return enabled_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveEnabled(const std::string& id) {
  // Only enabled extensions can be ready, so removing an enabled extension
  // should also remove from the ready set if possible.
  if (ready_extensions_.Contains(id))
    RemoveReady(id);
  return enabled_extensions_.Remove(id);
}

bool ExtensionRegistry::AddDisabled(
    const scoped_refptr<const Extension>& extension) {
  return disabled_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveDisabled(const std::string& id) {
  return disabled_extensions_.Remove(id);
}

bool ExtensionRegistry::AddTerminated(
    const scoped_refptr<const Extension>& extension) {
  return terminated_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveTerminated(const std::string& id) {
  return terminated_extensions_.Remove(id);
}

bool ExtensionRegistry::AddBlocklisted(
    const scoped_refptr<const Extension>& extension) {
  return blocklisted_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveBlocklisted(const std::string& id) {
  return blocklisted_extensions_.Remove(id);
}

bool ExtensionRegistry::AddBlocked(
    const scoped_refptr<const Extension>& extension) {
  return blocked_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveBlocked(const std::string& id) {
  return blocked_extensions_.Remove(id);
}

bool ExtensionRegistry::AddReady(
    const scoped_refptr<const Extension>& extension) {
  return ready_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveReady(const std::string& id) {
  return ready_extensions_.Remove(id);
}

void ExtensionRegistry::ClearAll() {
  enabled_extensions_.Clear();
  disabled_extensions_.Clear();
  terminated_extensions_.Clear();
  blocklisted_extensions_.Clear();
  blocked_extensions_.Clear();
  ready_extensions_.Clear();
}

void ExtensionRegistry::Shutdown() {
  // Release references to all Extension objects in the sets.
  ClearAll();
  for (auto& observer : observers_)
    observer.OnShutdown(this);
}

}  // namespace extensions
