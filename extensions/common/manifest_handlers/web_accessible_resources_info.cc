// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"

#include <stddef.h>

#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/api/web_accessible_resources.h"
#include "extensions/common/api/web_accessible_resources_mv2.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "url/url_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

const char kExtensionIdWildcard[] = "*";

using WebAccessibleResourcesManifestKeys =
    api::web_accessible_resources::ManifestKeys;
using WebAccessibleResourcesMv2ManifestKeys =
    api::web_accessible_resources_mv2::ManifestKeys;

namespace {

const WebAccessibleResourcesInfo* GetResourcesInfo(const Extension* extension) {
  return static_cast<WebAccessibleResourcesInfo*>(extension->GetManifestData(
      WebAccessibleResourcesManifestKeys::kWebAccessibleResources));
}

URLPattern GetPattern(std::string relative_path, const Extension& extension) {
  URLPattern pattern(URLPattern::SCHEME_EXTENSION);
  URLPattern::ParseResult result = pattern.Parse(extension.url().spec());
  DCHECK_EQ(URLPattern::ParseResult::kSuccess, result);
  while (relative_path[0] == '/')
    relative_path = relative_path.substr(1, relative_path.length() - 1);
  pattern.SetPath(pattern.path() + relative_path);
  return pattern;
}

std::unique_ptr<WebAccessibleResourcesInfo> ParseResourceStringList(
    const Extension& extension,
    std::u16string* error) {
  WebAccessibleResourcesMv2ManifestKeys manifest_keys;
  if (!WebAccessibleResourcesMv2ManifestKeys::ParseFromDictionary(
          extension.manifest()->available_values(), manifest_keys, *error)) {
    return nullptr;
  }

  auto info = std::make_unique<WebAccessibleResourcesInfo>();
  URLPatternSet resource_set;

  for (std::string& web_accessible_resource :
       manifest_keys.web_accessible_resources) {
    resource_set.AddPattern(
        GetPattern(std::move(web_accessible_resource), extension));
  }

  // In extensions where only a resource list is provided (as is the case in
  // manifest_version 2), resources are embeddable by any site. To handle
  // this, have |matches| match the specified schemes.
  URLPatternSet matches;
  matches.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, URLPattern::kAllUrlsPattern));
  info->web_accessible_resources.emplace_back(
      std::move(resource_set), std::move(matches), std::vector<ExtensionId>(),
      false, false);
  return info;
}

std::unique_ptr<WebAccessibleResourcesInfo> ParseEntryList(
    const Extension& extension,
    std::u16string* error) {
  auto info = std::make_unique<WebAccessibleResourcesInfo>();
  auto get_error = [](size_t i, std::string_view message) {
    return ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidWebAccessibleResource, base::NumberToString(i),
        message);
  };

  WebAccessibleResourcesManifestKeys manifest_keys;
  if (!WebAccessibleResourcesManifestKeys::ParseFromDictionary(
          extension.manifest()->available_values(), manifest_keys, *error)) {
    return nullptr;
  }

  size_t i = 0;
  for (auto& web_accessible_resource : manifest_keys.web_accessible_resources) {
    bool use_dynamic_url_bool = web_accessible_resource.use_dynamic_url &&
                                *web_accessible_resource.use_dynamic_url;

    if (!web_accessible_resource.matches &&
        !web_accessible_resource.extension_ids && !use_dynamic_url_bool) {
      *error = get_error(
          i, "Entry must at least have resources, and one other valid key.");
      return nullptr;
    }

    // Prepare each key of the web accessible resources.
    URLPatternSet resource_set;
    for (std::string& resource : web_accessible_resource.resources) {
      resource_set.AddPattern(GetPattern(std::move(resource), extension));
    }
    URLPatternSet match_set;
    if (web_accessible_resource.matches) {
      for (const std::string& match : *web_accessible_resource.matches) {
        URLPattern pattern(URLPattern::SCHEME_ALL);
        if (pattern.Parse(match) != URLPattern::ParseResult::kSuccess ||
            pattern.path() != "/*") {
          *error = get_error(i, "Invalid match pattern.");
          return nullptr;
        }
        match_set.AddPattern(pattern);
      }
    }

    // Extension IDs.
    std::vector<ExtensionId> extension_id_list;
    bool allow_all_extensions = false;
    if (web_accessible_resource.extension_ids) {
      extension_id_list.reserve(web_accessible_resource.extension_ids->size());
      for (ExtensionId& extension_id : *web_accessible_resource.extension_ids) {
        if (extension_id == kExtensionIdWildcard) {
          allow_all_extensions = true;
          continue;
        }
        if (!crx_file::id_util::IdIsValid(extension_id)) {
          *error = get_error(i, "Invalid extension id.");
          return nullptr;
        }
        extension_id_list.push_back(std::move(extension_id));
      }
      // If a wildcard is specified, only that value is allowed.
      if (allow_all_extensions &&
          web_accessible_resource.extension_ids->size() > 1) {
        *error = get_error(
            i, "If a wildcard entry is present, it must be the only entry.");
        return nullptr;
      }
    }

    info->web_accessible_resources.emplace_back(
        std::move(resource_set), std::move(match_set),
        std::move(extension_id_list), use_dynamic_url_bool,
        allow_all_extensions);
    ++i;
  }
  return info;
}

bool IsResourceWebAccessibleImpl(
    const Extension& extension,
    const std::optional<url::Origin>& initiator_origin,
    const GURL& upstream_url,
    const GURL& target_url) {
  std::string relative_path = target_url.path();

  // Set the intiator_url.
  GURL initiator_url;
  if (initiator_origin) {
    if (initiator_origin->opaque()) {
      initiator_url =
          initiator_origin->GetTupleOrPrecursorTupleIfOpaque().GetURL();
    } else {
      initiator_url = initiator_origin->GetURL();
    }
  }

  const WebAccessibleResourcesInfo* info = GetResourcesInfo(&extension);
  if (!info) {
    return false;
  }

  bool using_dynamic_url_extension_feature = base::FeatureList::IsEnabled(
      extensions_features::kExtensionDynamicURLRedirection);

  // Look for the first match in the array of web accessible resources.
  for (const auto& entry : info->web_accessible_resources) {
    if (extension.ResourceMatches(entry.resources, relative_path)) {
      bool result = true;

      // Prior to MV3, web-accessible resources were accessible by any site.
      // Preserve this behavior.
      if (extension.manifest_version() < 3) {
        return result;
      }

      // If `use_dynamic_url` is true in the manifest and the extension feature
      // is enabled, then only load the resource if the dynamic url is used. The
      // dynamic url should be ok to accept if it's a `host_piece` of either the
      // `upstream_url` or the `target_url` because the goal of this feature is
      // to ensure that the dynamic url was used for fetching the resource.
      if (using_dynamic_url_extension_feature && entry.use_dynamic_url) {
        bool is_guid_target_url = extension.guid() == target_url.host_piece();
        if (upstream_url.is_empty()) {
          result = is_guid_target_url;
        } else {
          result = extension.guid() == upstream_url.host_piece() ||
                   is_guid_target_url;
        }
        if (!result) {
          continue;
        }
      }

      // Determine if the `intiator_url` is allowed to access this resource.
      if (entry.matches.MatchesURL(initiator_url)) {
        return result;
      }

      // Allow if a wildcard was used, the initiator origin matches the
      // extension, or if the initiator host matches an entry extension id.
      if (initiator_url.SchemeIs(extensions::kExtensionScheme) &&
          (entry.allow_all_extensions ||
           extension.id() == initiator_url.host() ||
           base::Contains(entry.extension_ids, initiator_url.host()))) {
        return result;
      }
    }
  }

  // No match found.
  return false;
}

}  // namespace

WebAccessibleResourcesInfo::WebAccessibleResourcesInfo() = default;

WebAccessibleResourcesInfo::~WebAccessibleResourcesInfo() = default;

// static
// Returns true if the specified resource is web accessible.
bool WebAccessibleResourcesInfo::IsResourceWebAccessible(
    const Extension* extension,
    const std::string& relative_path,
    const url::Origin* initiator_origin) {
  CHECK(extension);
  return IsResourceWebAccessibleImpl(
      *extension, base::OptionalFromPtr(initiator_origin),
      /*upstream_url=*/GURL(),
      /*target_url=*/extension->GetResourceURL(relative_path));
}

// static
bool WebAccessibleResourcesInfo::IsResourceWebAccessibleRedirect(
    const Extension* extension,
    const GURL& target_url,
    const std::optional<url::Origin>& initiator_origin,
    const GURL& upstream_url) {
  CHECK(extension);
  CHECK(target_url.SchemeIs(kExtensionScheme));

  return IsResourceWebAccessibleImpl(*extension, initiator_origin, upstream_url,
                                     target_url);
}

// static
bool WebAccessibleResourcesInfo::HasWebAccessibleResources(
    const Extension* extension) {
  const WebAccessibleResourcesInfo* info = GetResourcesInfo(extension);
  return info && (info->web_accessible_resources.size() > 0);
}

// static
bool WebAccessibleResourcesInfo::ShouldUseDynamicUrl(const Extension* extension,
                                                     const std::string& path) {
  const WebAccessibleResourcesInfo* info = GetResourcesInfo(extension);
  if (!info) {
    return false;
  }
  for (const auto& entry : info->web_accessible_resources) {
    if (extension->ResourceMatches(entry.resources, path) &&
        entry.use_dynamic_url) {
      return true;
    }
  }
  return false;
}

WebAccessibleResourcesInfo::Entry::Entry() = default;

WebAccessibleResourcesInfo::Entry::Entry(
    WebAccessibleResourcesInfo::Entry&& rhs) = default;

WebAccessibleResourcesInfo::Entry::~Entry() = default;

WebAccessibleResourcesInfo::Entry::Entry(URLPatternSet resources,
                                         URLPatternSet matches,
                                         std::vector<ExtensionId> extension_ids,
                                         bool use_dynamic_url,
                                         bool allow_all_extensions)
    : resources(std::move(resources)),
      matches(std::move(matches)),
      extension_ids(std::move(extension_ids)),
      use_dynamic_url(use_dynamic_url),
      allow_all_extensions(allow_all_extensions) {}

WebAccessibleResourcesHandler::WebAccessibleResourcesHandler() = default;

WebAccessibleResourcesHandler::~WebAccessibleResourcesHandler() = default;

bool WebAccessibleResourcesHandler::Parse(Extension* extension,
                                          std::u16string* error) {
  auto info = extension->manifest_version() < 3
                  ? ParseResourceStringList(*extension, error)
                  : ParseEntryList(*extension, error);
  if (!info)
    return false;
  extension->SetManifestData(
      WebAccessibleResourcesManifestKeys::kWebAccessibleResources,
      std::move(info));
  return true;
}
base::span<const char* const> WebAccessibleResourcesHandler::Keys() const {
  static constexpr const char* kKeys[] = {
      WebAccessibleResourcesManifestKeys::kWebAccessibleResources};
  return kKeys;
}

}  // namespace extensions
