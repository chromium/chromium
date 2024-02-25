// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/automation.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/permission_message_util.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace errors = manifest_errors;
namespace keys = extensions::manifest_keys;
using api::extensions_manifest_types::Automation;

class AutomationManifestPermission : public ManifestPermission {
 public:
  explicit AutomationManifestPermission(
      std::unique_ptr<const AutomationInfo> automation_info)
      : automation_info_(std::move(automation_info)) {}

  // extensions::ManifestPermission overrides.
  std::string name() const override;

  std::string id() const override;

  PermissionIDSet GetPermissions() const override;

  bool FromValue(const base::Value* value) override;

  std::unique_ptr<base::Value> ToValue() const override;

  std::unique_ptr<ManifestPermission> Diff(
      const ManifestPermission* rhs) const override;

  std::unique_ptr<ManifestPermission> Union(
      const ManifestPermission* rhs) const override;

  std::unique_ptr<ManifestPermission> Intersect(
      const ManifestPermission* rhs) const override;

  bool RequiresManagementUIWarning() const override;

 private:
  std::unique_ptr<const AutomationInfo> automation_info_;
};

std::string AutomationManifestPermission::name() const {
  return keys::kAutomation;
}

std::string AutomationManifestPermission::id() const {
  return keys::kAutomation;
}

PermissionIDSet AutomationManifestPermission::GetPermissions() const {
  // Meant to mimic the behavior of GetMessages().
  PermissionIDSet permissions;
  if (automation_info_->desktop) {
    permissions.insert(APIPermissionID::kFullAccess);
  }
  return permissions;
}

bool AutomationManifestPermission::FromValue(const base::Value* value) {
  std::u16string error;
  automation_info_.reset(
      AutomationInfo::FromValue(*value, nullptr /* install_warnings */, &error)
          .release());
  return error.empty();
}

std::unique_ptr<base::Value> AutomationManifestPermission::ToValue() const {
  return AutomationInfo::ToValue(*automation_info_);
}

std::unique_ptr<ManifestPermission> AutomationManifestPermission::Diff(
    const ManifestPermission* rhs) const {
  const AutomationManifestPermission* other =
      static_cast<const AutomationManifestPermission*>(rhs);

  bool desktop = automation_info_->desktop && !other->automation_info_->desktop;
  return std::make_unique<AutomationManifestPermission>(
      base::WrapUnique(new const AutomationInfo(desktop)));
}

std::unique_ptr<ManifestPermission> AutomationManifestPermission::Union(
    const ManifestPermission* rhs) const {
  const AutomationManifestPermission* other =
      static_cast<const AutomationManifestPermission*>(rhs);

  bool desktop = automation_info_->desktop || other->automation_info_->desktop;
  return std::make_unique<AutomationManifestPermission>(
      base::WrapUnique(new const AutomationInfo(desktop)));
}

std::unique_ptr<ManifestPermission> AutomationManifestPermission::Intersect(
    const ManifestPermission* rhs) const {
  const AutomationManifestPermission* other =
      static_cast<const AutomationManifestPermission*>(rhs);

  bool desktop = automation_info_->desktop && other->automation_info_->desktop;
  return std::make_unique<AutomationManifestPermission>(
      base::WrapUnique(new const AutomationInfo(desktop)));
}

bool AutomationManifestPermission::RequiresManagementUIWarning() const {
  return automation_info_->desktop;
}

AutomationHandler::AutomationHandler() = default;

AutomationHandler::~AutomationHandler() = default;

bool AutomationHandler::Parse(Extension* extension, std::u16string* error) {
  const base::Value* automation =
      extension->manifest()->FindPath(keys::kAutomation);
  CHECK(automation != nullptr);
  std::vector<InstallWarning> install_warnings;
  std::unique_ptr<AutomationInfo> info =
      AutomationInfo::FromValue(*automation, &install_warnings, error);
  if (!error->empty())
    return false;

  extension->AddInstallWarnings(std::move(install_warnings));

  if (!info)
    return true;

  extension->SetManifestData(keys::kAutomation, std::move(info));
  return true;
}

base::span<const char* const> AutomationHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kAutomation};
  return kKeys;
}

ManifestPermission* AutomationHandler::CreatePermission() {
  return new AutomationManifestPermission(
      base::WrapUnique(new const AutomationInfo));
}

ManifestPermission* AutomationHandler::CreateInitialRequiredPermission(
    const Extension* extension) {
  const AutomationInfo* info = AutomationInfo::Get(extension);
  if (info) {
    return new AutomationManifestPermission(
        base::WrapUnique(new const AutomationInfo(info->desktop)));
  }
  return nullptr;
}

// static
const AutomationInfo* AutomationInfo::Get(const Extension* extension) {
  return static_cast<AutomationInfo*>(
      extension->GetManifestData(keys::kAutomation));
}

// static
std::unique_ptr<AutomationInfo> AutomationInfo::FromValue(
    const base::Value& value,
    std::vector<InstallWarning>* install_warnings,
    std::u16string* error) {
  auto automation = Automation::FromValue(value);
  if (!automation.has_value()) {
    *error = std::move(automation).error();
    return nullptr;
  }

  if (automation->as_boolean) {
    if (*automation->as_boolean)
      return base::WrapUnique(new AutomationInfo());
    return nullptr;
  }
  const Automation::Object& automation_object = *automation->as_object;

  bool desktop = false;
  if (automation_object.desktop && *automation_object.desktop) {
    desktop = true;
  }

  return base::WrapUnique(new AutomationInfo(desktop));
}

// static
std::unique_ptr<base::Value> AutomationInfo::ToValue(
    const AutomationInfo& info) {
  return base::Value::ToUniquePtrValue(AsManifestType(info)->ToValue());
}

// static
std::unique_ptr<Automation> AutomationInfo::AsManifestType(
    const AutomationInfo& info) {
  std::unique_ptr<Automation> automation(new Automation);
  if (!info.desktop) {
    automation->as_boolean = true;
    return automation;
  }

  automation->as_object.emplace();
  automation->as_object->desktop = info.desktop;

  return automation;
}

AutomationInfo::AutomationInfo() : desktop(false) {}

AutomationInfo::AutomationInfo(bool desktop) : desktop(desktop) {}

AutomationInfo::~AutomationInfo() = default;

}  // namespace extensions
