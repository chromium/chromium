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
#include "extensions/common/url_pattern.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace automation_errors {
const char kErrorDesktopTrueInteractFalse[] =
    "Cannot specify interactive=false if desktop=true is specified; "
    "interactive=false will be ignored.";
const char kErrorDesktopTrueMatchesSpecified[] =
    "Cannot specify matches for Automation if desktop=true is specified; "
    "matches will be ignored.";
const char kErrorInvalidMatch[] = "Invalid match pattern '*': *";
const char kErrorNoMatchesProvided[] = "No valid match patterns provided.";
}  // namespace automation_errors

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
  } else if (automation_info_->matches.MatchesAllURLs()) {
    if (automation_info_->interact) {
      permissions.insert(APIPermissionID::kHostsAll);
    } else {
      permissions.insert(APIPermissionID::kHostsAllReadOnly);
    }
  } else {
    // Check if we get any additional permissions from FilterHostPermissions.
    URLPatternSet regular_hosts;
    ExtensionsClient::Get()->FilterHostPermissions(
        automation_info_->matches, &regular_hosts, &permissions);
    std::set<std::string> hosts =
        permission_message_util::GetDistinctHosts(regular_hosts, true, true);
    APIPermissionID permission_id = automation_info_->interact
                                        ? APIPermissionID::kHostReadWrite
                                        : APIPermissionID::kHostReadOnly;
    for (const auto& host : hosts)
      permissions.insert(permission_id, base::UTF8ToUTF16(host));
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
  bool interact =
      automation_info_->interact && !other->automation_info_->interact;
  URLPatternSet matches = URLPatternSet::CreateDifference(
      automation_info_->matches, other->automation_info_->matches);
  return std::make_unique<AutomationManifestPermission>(
      base::WrapUnique(new const AutomationInfo(desktop, matches, interact)));
}

std::unique_ptr<ManifestPermission> AutomationManifestPermission::Union(
    const ManifestPermission* rhs) const {
  const AutomationManifestPermission* other =
      static_cast<const AutomationManifestPermission*>(rhs);

  bool desktop = automation_info_->desktop || other->automation_info_->desktop;
  bool interact =
      automation_info_->interact || other->automation_info_->interact;
  URLPatternSet matches = URLPatternSet::CreateUnion(
      automation_info_->matches, other->automation_info_->matches);
  return std::make_unique<AutomationManifestPermission>(
      base::WrapUnique(new const AutomationInfo(desktop, matches, interact)));
}

std::unique_ptr<ManifestPermission> AutomationManifestPermission::Intersect(
    const ManifestPermission* rhs) const {
  const AutomationManifestPermission* other =
      static_cast<const AutomationManifestPermission*>(rhs);

  bool desktop = automation_info_->desktop && other->automation_info_->desktop;
  bool interact =
      automation_info_->interact && other->automation_info_->interact;
  URLPatternSet matches = URLPatternSet::CreateIntersection(
      automation_info_->matches, other->automation_info_->matches,
      URLPatternSet::IntersectionBehavior::kStringComparison);
  return std::make_unique<AutomationManifestPermission>(
      base::WrapUnique(new const AutomationInfo(desktop, matches, interact)));
}

bool AutomationManifestPermission::RequiresManagementUIWarning() const {
  return automation_info_->desktop || !automation_info_->matches.is_empty();
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
        base::WrapUnique(new const AutomationInfo(info->desktop, info->matches,
                                                  info->interact)));
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
  std::unique_ptr<Automation> automation =
      Automation::FromValueDeprecated(value, error);
  if (!automation)
    return nullptr;

  if (automation->as_boolean) {
    if (*automation->as_boolean)
      return base::WrapUnique(new AutomationInfo());
    return nullptr;
  }
  const Automation::Object& automation_object = *automation->as_object;

  bool desktop = false;
  bool interact = false;
  if (automation_object.desktop && *automation_object.desktop) {
    desktop = true;
    interact = true;
    if (automation_object.interact && !*automation_object.interact) {
      // TODO(aboxhall): Do we want to allow this?
      install_warnings->emplace_back(
          automation_errors::kErrorDesktopTrueInteractFalse);
    }
  } else if (automation_object.interact && *automation_object.interact) {
    interact = true;
  }

  URLPatternSet matches;
  bool specified_matches = false;
  if (automation_object.matches) {
    if (desktop) {
      install_warnings->emplace_back(
          automation_errors::kErrorDesktopTrueMatchesSpecified);
    } else {
      specified_matches = true;

      for (const auto& match : *automation_object.matches) {
        // TODO(aboxhall): Refactor common logic from content_scripts_handler,
        // manifest_url_handler and user_script.cc into a single location and
        // re-use here.
        URLPattern pattern(URLPattern::SCHEME_ALL &
                           ~URLPattern::SCHEME_CHROMEUI);
        URLPattern::ParseResult parse_result = pattern.Parse(match);

        if (parse_result != URLPattern::ParseResult::kSuccess) {
          install_warnings->emplace_back(ErrorUtils::FormatErrorMessage(
              automation_errors::kErrorInvalidMatch, match,
              URLPattern::GetParseResultString(parse_result)));
          continue;
        }

        matches.AddPattern(pattern);
      }
    }
  }
  if (specified_matches && matches.is_empty()) {
    install_warnings->emplace_back(automation_errors::kErrorNoMatchesProvided);
  }

  return base::WrapUnique(new AutomationInfo(desktop, matches, interact));
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
  if (!info.desktop && !info.interact && info.matches.size() == 0) {
    automation->as_boolean = true;
    return automation;
  }

  automation->as_object.emplace();
  automation->as_object->desktop = info.desktop;
  automation->as_object->interact = info.interact;
  if (info.matches.size() > 0)
    automation->as_object->matches = info.matches.ToStringVector();

  return automation;
}

AutomationInfo::AutomationInfo() : desktop(false), interact(false) {}

AutomationInfo::AutomationInfo(bool desktop,
                               const URLPatternSet& matches,
                               bool interact)
    : desktop(desktop), matches(matches.Clone()), interact(interact) {}

AutomationInfo::~AutomationInfo() = default;

}  // namespace extensions
