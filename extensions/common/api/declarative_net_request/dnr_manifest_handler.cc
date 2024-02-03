// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/dnr_manifest_handler.h"

#include <set>
#include <string_view>

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "tools/json_schema_compiler/util.h"

namespace extensions {

namespace errors = manifest_errors;
namespace dnr_api = api::declarative_net_request;

namespace declarative_net_request {

DNRManifestHandler::DNRManifestHandler() = default;
DNRManifestHandler::~DNRManifestHandler() = default;

bool DNRManifestHandler::Parse(Extension* extension, std::u16string* error) {
  DCHECK(extension->manifest()->FindKey(
      dnr_api::ManifestKeys::kDeclarativeNetRequest));

  bool has_permission =
      PermissionsParser::HasAPIPermission(
          extension, mojom::APIPermissionID::kDeclarativeNetRequest) ||
      PermissionsParser::HasAPIPermission(
          extension,
          mojom::APIPermissionID::kDeclarativeNetRequestWithHostAccess);
  if (!has_permission) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kDeclarativeNetRequestPermissionNeeded,
        dnr_api::ManifestKeys::kDeclarativeNetRequest);
    return false;
  }

  dnr_api::ManifestKeys manifest_keys;
  if (!dnr_api::ManifestKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys, *error)) {
    return false;
  }
  std::vector<dnr_api::Ruleset> rulesets =
      std::move(manifest_keys.declarative_net_request.rule_resources);

  if (rulesets.size() >
      static_cast<size_t>(dnr_api::MAX_NUMBER_OF_STATIC_RULESETS)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kRulesetCountExceeded,
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        dnr_api::DNRInfo::kRuleResources,
        base::NumberToString(dnr_api::MAX_NUMBER_OF_STATIC_RULESETS));
    return false;
  }

  std::set<std::string_view> ruleset_ids;

  // Validates the ruleset at the given |index|. On success, returns true and
  // populates |info|. On failure, returns false and populates |error|.
  auto get_ruleset_info = [extension, error, &rulesets, &ruleset_ids](
                              int index, DNRManifestData::RulesetInfo* info) {
    // Path validation.
    ExtensionResource resource = extension->GetResource(rulesets[index].path);
    if (resource.empty() || resource.relative_path().ReferencesParent()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kRulesFileIsInvalid,
          dnr_api::ManifestKeys::kDeclarativeNetRequest,
          dnr_api::DNRInfo::kRuleResources, rulesets[index].path);
      return false;
    }

    // ID validation.
    const std::string& manifest_id = rulesets[index].id;

    // Sanity check that the dynamic and session ruleset IDs are reserved.
    DCHECK_EQ(kReservedRulesetIDPrefix, dnr_api::DYNAMIC_RULESET_ID[0]);
    DCHECK_EQ(kReservedRulesetIDPrefix, dnr_api::SESSION_RULESET_ID[0]);

    if (manifest_id.empty() || !ruleset_ids.insert(manifest_id).second ||
        manifest_id[0] == kReservedRulesetIDPrefix) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidRulesetID,
          dnr_api::ManifestKeys::kDeclarativeNetRequest,
          dnr_api::DNRInfo::kRuleResources, base::NumberToString(index));
      return false;
    }

    info->relative_path = resource.relative_path().NormalizePathSeparators();

    info->id = RulesetID(kMinValidStaticRulesetID.value() + index);

    info->enabled = rulesets[index].enabled;
    info->manifest_id = manifest_id;
    return true;
  };

  std::vector<DNRManifestData::RulesetInfo> rulesets_info;
  rulesets_info.reserve(rulesets.size());

  int enabled_ruleset_count = 0;

  std::set<base::FilePath> unique_paths;
  bool unique_paths_warning = false;

  // Note: the static_cast<int> below is safe because we did already verify that
  // |rulesets.size()| <= dnr_api::MAX_NUMBER_OF_STATIC_RULESETS, which is an
  // integer.
  for (int i = 0; i < static_cast<int>(rulesets.size()); ++i) {
    DNRManifestData::RulesetInfo info;
    if (!get_ruleset_info(i, &info))
      return false;

    if (info.enabled)
      enabled_ruleset_count++;

    // The DNR system can support assigning different IDs to a ruleset, but this
    // is likely a developers' mistake since two declarations of the same rule
    // consume twice the amount of resources.
    if (!unique_paths_warning) {
      if (unique_paths.contains(info.relative_path)) {
        unique_paths_warning = true;
        extension->AddInstallWarning(
            InstallWarning(errors::kDeclarativeNetRequestPathDuplicates,
                           dnr_api::ManifestKeys::kDeclarativeNetRequest));
      } else {
        unique_paths.insert(info.relative_path);
      }
    }

    rulesets_info.push_back(std::move(info));
  }

  if (enabled_ruleset_count > dnr_api::MAX_NUMBER_OF_ENABLED_STATIC_RULESETS) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kEnabledRulesetCountExceeded,
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        dnr_api::DNRInfo::kRuleResources,
        base::NumberToString(dnr_api::MAX_NUMBER_OF_ENABLED_STATIC_RULESETS));
    return false;
  }

  extension->SetManifestData(
      dnr_api::ManifestKeys::kDeclarativeNetRequest,
      std::make_unique<DNRManifestData>(std::move(rulesets_info)));
  return true;
}

bool DNRManifestHandler::Validate(const Extension* extension,
                                  std::string* error,
                                  std::vector<InstallWarning>* warnings) const {
  DNRManifestData* data =
      static_cast<DNRManifestData*>(extension->GetManifestData(
          dnr_api::ManifestKeys::kDeclarativeNetRequest));
  DCHECK(data);

  for (const DNRManifestData::RulesetInfo& info : data->rulesets) {
    // Check file path validity. We don't use Extension::GetResource since it
    // returns a failure if the relative path contains Windows path separators
    // and we have already normalized the path separators.
    if (ExtensionResource::GetFilePath(
            extension->path(), info.relative_path,
            ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT)
            .empty()) {
      *error = ErrorUtils::FormatErrorMessage(
          errors::kRulesFileIsInvalid,
          dnr_api::ManifestKeys::kDeclarativeNetRequest,
          dnr_api::DNRInfo::kRuleResources, info.relative_path.AsUTF8Unsafe());
      return false;
    }
  }

  return true;
}

base::span<const char* const> DNRManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {
      dnr_api::ManifestKeys::kDeclarativeNetRequest};
  return kKeys;
}

}  // namespace declarative_net_request
}  // namespace extensions
