// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handler.h"

#include <stddef.h>

#include <map>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler_registry.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/manifest_permission_set.h"

namespace extensions {

ManifestHandler::ManifestHandler() = default;

ManifestHandler::~ManifestHandler() = default;

bool ManifestHandler::Validate(const Extension* extension,
                               std::string* error,
                               std::vector<InstallWarning>* warnings) const {
  return true;
}

bool ManifestHandler::AlwaysParseForType(Manifest::Type type) const {
  return false;
}

bool ManifestHandler::AlwaysValidateForType(Manifest::Type type) const {
  return false;
}

const std::vector<std::string> ManifestHandler::PrerequisiteKeys() const {
  return std::vector<std::string>();
}

ManifestPermission* ManifestHandler::CreatePermission() {
  return nullptr;
}

ManifestPermission* ManifestHandler::CreateInitialRequiredPermission(
    const Extension* extension) {
  return nullptr;
}

// static
void ManifestHandler::FinalizeRegistration() {
  ManifestHandlerRegistry::Get()->Finalize();
}

// static
bool ManifestHandler::IsRegistrationFinalized() {
  return ManifestHandlerRegistry::Get()->is_finalized_;
}

// static
bool ManifestHandler::ParseExtension(Extension* extension,
                                     std::u16string* error) {
  return ManifestHandlerRegistry::Get()->ParseExtension(extension, error);
}

// static
bool ManifestHandler::ValidateExtension(const Extension* extension,
                                        std::string* error,
                                        std::vector<InstallWarning>* warnings) {
  return ManifestHandlerRegistry::Get()->ValidateExtension(extension, error,
                                                           warnings);
}

// static
ManifestPermission* ManifestHandler::CreatePermission(const std::string& name) {
  return ManifestHandlerRegistry::Get()->CreatePermission(name);
}

// static
void ManifestHandler::AddExtensionInitialRequiredPermissions(
    const Extension* extension, ManifestPermissionSet* permission_set) {
  return ManifestHandlerRegistry::Get()->AddExtensionInitialRequiredPermissions(
      extension, permission_set);
}

// static
const std::vector<std::string> ManifestHandler::SingleKey(
    const std::string& key) {
  return std::vector<std::string>(1, key);
}

}  // namespace extensions
