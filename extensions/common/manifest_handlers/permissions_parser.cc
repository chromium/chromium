// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/permissions_parser.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern_set.h"
#include "url/url_constants.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

struct ManifestPermissions : public Extension::ManifestData {
  ManifestPermissions(std::unique_ptr<const PermissionSet> permissions);
  ~ManifestPermissions() override;

  std::unique_ptr<const PermissionSet> permissions;
};

ManifestPermissions::ManifestPermissions(
    std::unique_ptr<const PermissionSet> permissions)
    : permissions(std::move(permissions)) {}

ManifestPermissions::~ManifestPermissions() {
}

// Checks whether the host |pattern| is allowed for the given |extension|,
// given API permissions |permissions|.
bool CanSpecifyHostPermission(const Extension* extension,
                              const URLPattern& pattern,
                              const APIPermissionSet& permissions) {
  if (!pattern.match_all_urls() &&
      pattern.MatchesScheme(content::kChromeUIScheme)) {
    URLPatternSet chrome_scheme_hosts =
        ExtensionsClient::Get()->GetPermittedChromeSchemeHosts(extension,
                                                               permissions);
    if (chrome_scheme_hosts.ContainsPattern(pattern))
      return true;

    // Component extensions can have access to all of chrome://*.
    if (PermissionsData::CanExecuteScriptEverywhere(extension->id(),
                                                    extension->location())) {
      return true;
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kExtensionsOnChromeURLs)) {
      return true;
    }

    // TODO(aboxhall): return from_webstore() when webstore handles blocking
    // extensions which request chrome:// urls
    return false;
  }

  // Otherwise, the valid schemes were handled by URLPattern.
  return true;
}

// Parses hosts from `key` in the extension's manifest into |hosts|.
bool ParseHostsFromJSON(Extension* extension,
                        const char* key,
                        std::vector<std::string>* hosts,
                        std::u16string* error) {
  if (!extension->manifest()->FindKey(key))
    return true;

  const base::Value* permissions = nullptr;
  if (!extension->manifest()->GetList(key, &permissions)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidHostPermissions, key);
    return false;
  }

  // Add all permissions parsed from the manifest to |hosts|.
  const base::Value::List& list = permissions->GetList();
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i].is_string()) {
      hosts->push_back(list[i].GetString());
    } else {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidHostPermission, key, base::NumberToString(i));
      return false;
    }
  }

  return true;
}

void ParseHostPermissions(Extension* extension,
                          const char* key,
                          const std::vector<std::string>& host_data,
                          const APIPermissionSet& api_permissions,
                          URLPatternSet* host_permissions) {
  bool can_execute_script_everywhere =
      PermissionsData::CanExecuteScriptEverywhere(extension->id(),
                                                  extension->location());

  // Users should be able to enable file access for extensions with activeTab.
  if (!can_execute_script_everywhere &&
      base::Contains(api_permissions, APIPermissionID::kActiveTab)) {
    extension->set_wants_file_access(true);
  }

  const int kAllowedSchemes = can_execute_script_everywhere
                                  ? URLPattern::SCHEME_ALL
                                  : Extension::kValidHostPermissionSchemes;

  const bool all_urls_includes_chrome_urls =
      PermissionsData::AllUrlsIncludesChromeUrls(extension->id());

  for (std::vector<std::string>::const_iterator iter = host_data.begin();
       iter != host_data.end(); ++iter) {
    const std::string& permission_str = *iter;

    // Check if it's a host pattern permission.
    URLPattern pattern = URLPattern(kAllowedSchemes);
    URLPattern::ParseResult parse_result = pattern.Parse(permission_str);
    if (parse_result == URLPattern::ParseResult::kSuccess) {
      // The path component is not used for host permissions, so we force it
      // to match all paths.
      pattern.SetPath("/*");
      int valid_schemes = pattern.valid_schemes();
      if (pattern.MatchesScheme(url::kFileScheme) &&
          !can_execute_script_everywhere) {
        extension->set_wants_file_access(true);
        if (!(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS))
          valid_schemes &= ~URLPattern::SCHEME_FILE;
      }

      if (pattern.scheme() != content::kChromeUIScheme &&
          !all_urls_includes_chrome_urls) {
        // Keep chrome:// in allowed schemes only if it's explicitly requested
        // or been granted by extension ID. If the extensions_on_chrome_urls
        // flag is not set, CanSpecifyHostPermission will fail, so don't check
        // the flag here.
        valid_schemes &= ~URLPattern::SCHEME_CHROMEUI;
      }
      pattern.SetValidSchemes(valid_schemes);

      if (!CanSpecifyHostPermission(extension, pattern, api_permissions)) {
        // TODO(aboxhall): make a warning (see pattern.match_all_urls() block
        // below).
        extension->AddInstallWarning(InstallWarning(
            ErrorUtils::FormatErrorMessage(errors::kInvalidPermissionScheme,
                                           key, permission_str),
            key, permission_str));
        continue;
      }

      host_permissions->AddPattern(pattern);
      // We need to make sure all_urls matches any allowed Chrome-schemed hosts,
      // so add them back in to host_permissions separately.
      if (pattern.match_all_urls()) {
        host_permissions->AddPatterns(
            ExtensionsClient::Get()->GetPermittedChromeSchemeHosts(
                extension, api_permissions));
      }
      continue;
    }

    // It's probably an unknown API permission. Do not throw an error so
    // extensions can retain backwards compatibility (http://crbug.com/42742).
    extension->AddInstallWarning(InstallWarning(
        ErrorUtils::FormatErrorMessage(
            manifest_errors::kPermissionUnknownOrMalformed, permission_str),
        key, permission_str));
  }
}

// Parses the host and api permissions from the specified permission |key|
// from |extension|'s manifest.
bool ParseHelper(Extension* extension,
                 const char* key,
                 APIPermissionSet* api_permissions,
                 URLPatternSet* host_permissions,
                 std::u16string* error) {
  if (!extension->manifest()->FindKey(key))
    return true;

  const base::Value* permissions = nullptr;
  if (!extension->manifest()->GetList(key, &permissions)) {
    *error = errors::kInvalidPermissions;
    return false;
  }

  // NOTE: We need to get the APIPermission before we check if features
  // associated with them are available because the feature system does not
  // know about aliases.

  std::vector<std::string> host_data;
  if (!APIPermissionSet::ParseFromJSON(
          permissions->GetList(),
          APIPermissionSet::kDisallowInternalPermissions, api_permissions,
          error, &host_data)) {
    return false;
  }

  // Verify feature availability of permissions.
  std::vector<APIPermissionID> to_remove;
  const FeatureProvider* permission_features =
      FeatureProvider::GetPermissionFeatures();
  for (APIPermissionSet::const_iterator iter = api_permissions->begin();
       iter != api_permissions->end();
       ++iter) {
    // All internal permissions should have been filtered out above.
    DCHECK(!iter->info()->is_internal()) << iter->name();

    const Feature* feature = permission_features->GetFeature(iter->name());

    // The feature should exist since we just got an APIPermission for it. The
    // two systems should be updated together whenever a permission is added.
    DCHECK(feature) << "Could not find feature for " << iter->name();
    // http://crbug.com/176381
    if (!feature) {
      to_remove.push_back(iter->id());
      continue;
    }

    // Sneaky check for "experimental", which we always allow for extensions
    // installed from the Webstore. This way we can allowlist extensions to
    // have access to experimental in just the store, and not have to push a
    // new version of the client. Otherwise, experimental goes through the
    // usual features check.
    if (iter->id() == APIPermissionID::kExperimental &&
        extension->from_webstore()) {
      continue;
    }

    Feature::Availability availability =
        feature->IsAvailableToExtension(extension);
    if (!availability.is_available()) {
      // Don't fail, but warn the developer that the manifest contains
      // unrecognized permissions. This may happen legitimately if the
      // extensions requests platform- or channel-specific permissions.
      extension->AddInstallWarning(
          InstallWarning(availability.message(), feature->name()));
      to_remove.push_back(iter->id());
      continue;
    }
  }

  // Remove permissions that are not available to this extension.
  for (std::vector<APIPermissionID>::const_iterator iter = to_remove.begin();
       iter != to_remove.end(); ++iter) {
    api_permissions->erase(*iter);
  }

  if (extension->manifest_version() < 3) {
    ParseHostPermissions(extension, key, host_data, *api_permissions,
                         host_permissions);
  } else {
    // Iterate through unhandled permissions (in |host_data|) and add an install
    // warning for each.
    for (const auto& permission_str : host_data) {
      extension->AddInstallWarning(InstallWarning(
          ErrorUtils::FormatErrorMessage(
              manifest_errors::kPermissionUnknownOrMalformed, permission_str),
          key, permission_str));
    }
  }

  return true;
}

void RemoveNonAllowedOptionalPermissions(
    Extension* extension,
    APIPermissionSet* optional_api_permissions) {
  std::vector<InstallWarning> install_warnings;
  std::set<APIPermissionID> ids_to_erase;

  for (const auto* api_permission : *optional_api_permissions) {
    if (api_permission->info()->supports_optional())
      continue;
    // A permission that doesn't support being optional was listed in optional
    // permissions. Add a warning, and slate it for removal from the set.
    install_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(
            manifest_errors::kPermissionCannotBeOptional,
            api_permission->name()),
        keys::kOptionalPermissions, api_permission->name());
    ids_to_erase.insert(api_permission->id());
  }

  DCHECK_EQ(install_warnings.size(), ids_to_erase.size());
  if (!install_warnings.empty()) {
    extension->AddInstallWarnings(std::move(install_warnings));
    for (auto id : ids_to_erase) {
      size_t erased = optional_api_permissions->erase(id);
      DCHECK_EQ(1u, erased);
    }
  }
}

void RemoveOverlappingAPIPermissions(
    Extension* extension,
    const APIPermissionSet& required_api_permissions,
    APIPermissionSet* optional_api_permissions) {
  APIPermissionSet overlapping_api_permissions;
  APIPermissionSet::Intersection(required_api_permissions,
                                 *optional_api_permissions,
                                 &overlapping_api_permissions);

  if (overlapping_api_permissions.empty())
    return;

  std::vector<InstallWarning> install_warnings;
  install_warnings.reserve(overlapping_api_permissions.size());

  for (const auto* api_permission : overlapping_api_permissions) {
    install_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(
            manifest_errors::kPermissionMarkedOptionalAndRequired,
            api_permission->name()),
        keys::kOptionalPermissions, api_permission->name());
  }

  extension->AddInstallWarnings(std::move(install_warnings));

  APIPermissionSet new_optional_api_permissions;
  APIPermissionSet::Difference(*optional_api_permissions,
                               required_api_permissions,
                               &new_optional_api_permissions);

  *optional_api_permissions = std::move(new_optional_api_permissions);
}

void RemoveOverlappingHostPermissions(
    Extension* extension,
    const URLPatternSet& required_host_permissions,
    URLPatternSet* optional_host_permissions) {
  URLPatternSet new_optional_host_permissions;
  std::vector<InstallWarning> install_warnings;
  const char* key = extension->manifest_version() >= 3
                        ? keys::kOptionalHostPermissions
                        : keys::kOptionalPermissions;

  for (const URLPattern& host_permission : *optional_host_permissions) {
    if (required_host_permissions.ContainsPattern(host_permission)) {
      // We have detected a URLPattern in the optional hosts permission set that
      // is a strict subset of at least one URLPattern in the required hosts
      // permission set so we add an install warning.
      install_warnings.emplace_back(
          ErrorUtils::FormatErrorMessage(
              manifest_errors::kPermissionMarkedOptionalAndRequired,
              host_permission.GetAsString()),
          key);
    } else {
      new_optional_host_permissions.AddPattern(host_permission);
    }
  }

  if (!install_warnings.empty())
    extension->AddInstallWarnings(std::move(install_warnings));

  *optional_host_permissions = std::move(new_optional_host_permissions);
}

}  // namespace

struct PermissionsParser::InitialPermissions {
  APIPermissionSet api_permissions;
  ManifestPermissionSet manifest_permissions;
  URLPatternSet host_permissions;
  URLPatternSet scriptable_hosts;
};

PermissionsParser::PermissionsParser() {
}

PermissionsParser::~PermissionsParser() {
}

bool PermissionsParser::Parse(Extension* extension, std::u16string* error) {
  initial_required_permissions_ = std::make_unique<InitialPermissions>();
  if (!ParseHelper(extension,
                   keys::kPermissions,
                   &initial_required_permissions_->api_permissions,
                   &initial_required_permissions_->host_permissions,
                   error)) {
    return false;
  }

  initial_optional_permissions_ = std::make_unique<InitialPermissions>();
  if (!ParseHelper(extension, keys::kOptionalPermissions,
                   &initial_optional_permissions_->api_permissions,
                   &initial_optional_permissions_->host_permissions, error)) {
    return false;
  }

  if (extension->manifest_version() >= 3) {
    std::vector<std::string> manifest_hosts;
    std::vector<std::string> manifest_optional_hosts;
    if (!ParseHostsFromJSON(extension, keys::kHostPermissions, &manifest_hosts,
                            error)) {
      return false;
    }

    if (!ParseHostsFromJSON(extension, keys::kOptionalHostPermissions,
                            &manifest_optional_hosts, error)) {
      return false;
    }

    // TODO(kelvinjiang): Remove the dependency for |api_permissions| here.
    ParseHostPermissions(extension, keys::kHostPermissions, manifest_hosts,
                         initial_required_permissions_->api_permissions,
                         &initial_required_permissions_->host_permissions);

    ParseHostPermissions(extension, keys::kOptionalHostPermissions,
                         manifest_optional_hosts,
                         initial_optional_permissions_->api_permissions,
                         &initial_optional_permissions_->host_permissions);
  }

  // Remove and add install warnings for specified optional API permissions
  // which don't support being optional.
  RemoveNonAllowedOptionalPermissions(
      extension, &initial_optional_permissions_->api_permissions);

  // If permissions are specified as both required and optional
  // add an install warning for each permission and remove them from the
  // optional set while keeping them in the required set.
  RemoveOverlappingAPIPermissions(
      extension, initial_required_permissions_->api_permissions,
      &initial_optional_permissions_->api_permissions);

  RemoveOverlappingHostPermissions(
      extension, initial_required_permissions_->host_permissions,
      &initial_optional_permissions_->host_permissions);

  return true;
}

void PermissionsParser::Finalize(Extension* extension) {
  ManifestHandler::AddExtensionInitialRequiredPermissions(
      extension, &initial_required_permissions_->manifest_permissions);

  // TODO(devlin): Make this destructive and std::move() from initial
  // permissions so we can std::move() the sets.
  std::unique_ptr<const PermissionSet> required_permissions(new PermissionSet(
      initial_required_permissions_->api_permissions.Clone(),
      initial_required_permissions_->manifest_permissions.Clone(),
      initial_required_permissions_->host_permissions.Clone(),
      initial_required_permissions_->scriptable_hosts.Clone()));
  extension->SetManifestData(
      keys::kPermissions,
      std::make_unique<ManifestPermissions>(std::move(required_permissions)));

  std::unique_ptr<const PermissionSet> optional_permissions(new PermissionSet(
      initial_optional_permissions_->api_permissions.Clone(),
      initial_optional_permissions_->manifest_permissions.Clone(),
      initial_optional_permissions_->host_permissions.Clone(),
      URLPatternSet()));
  extension->SetManifestData(
      keys::kOptionalPermissions,
      std::make_unique<ManifestPermissions>(std::move(optional_permissions)));
}

// static
void PermissionsParser::AddAPIPermission(Extension* extension,
                                         APIPermissionID permission) {
  DCHECK(extension->permissions_parser());
  extension->permissions_parser()
      ->initial_required_permissions_->api_permissions.insert(permission);
}

// static
void PermissionsParser::AddAPIPermission(Extension* extension,
                                         APIPermission* permission) {
  DCHECK(extension->permissions_parser());
  extension->permissions_parser()
      ->initial_required_permissions_->api_permissions.insert(
          base::WrapUnique(permission));
}

// static
bool PermissionsParser::HasAPIPermission(const Extension* extension,
                                         APIPermissionID permission) {
  DCHECK(extension->permissions_parser());
  return extension->permissions_parser()
             ->initial_required_permissions_->api_permissions.count(
                 permission) > 0;
}

// static
void PermissionsParser::SetScriptableHosts(
    Extension* extension,
    const URLPatternSet& scriptable_hosts) {
  DCHECK(extension->permissions_parser());
  extension->permissions_parser()
      ->initial_required_permissions_->scriptable_hosts =
      scriptable_hosts.Clone();
}

// static
const PermissionSet& PermissionsParser::GetRequiredPermissions(
    const Extension* extension) {
  DCHECK(extension->GetManifestData(keys::kPermissions));
  return *static_cast<const ManifestPermissions*>(
              extension->GetManifestData(keys::kPermissions))
              ->permissions;
}

// static
const PermissionSet& PermissionsParser::GetOptionalPermissions(
    const Extension* extension) {
  DCHECK(extension->GetManifestData(keys::kOptionalPermissions));
  return *static_cast<const ManifestPermissions*>(
              extension->GetManifestData(keys::kOptionalPermissions))
              ->permissions;
}

}  // namespace extensions
