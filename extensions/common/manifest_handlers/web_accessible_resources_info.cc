// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

const WebAccessibleResourcesInfo* GetResourcesInfo(const Extension* extension) {
  return static_cast<WebAccessibleResourcesInfo*>(
      extension->GetManifestData(keys::kWebAccessibleResources));
}

base::Optional<URLPattern> GetPatternOrError(const base::Value& path,
                                             const Extension& extension,
                                             const size_t i,
                                             base::string16* error) {
  URLPattern pattern(URLPattern::SCHEME_EXTENSION);
  if (!path.is_string()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidWebAccessibleResource, base::NumberToString(i));
    return base::nullopt;
  }
  std::string relative_path = path.GetString();
  if (pattern.Parse(extension.url().spec()) !=
      URLPattern::ParseResult::kSuccess) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidURLPatternError, extension.url().spec());
    return base::nullopt;
  }
  while (relative_path[0] == '/')
    relative_path = relative_path.substr(1, relative_path.length() - 1);
  pattern.SetPath(pattern.path() + relative_path);
  return pattern;
}

std::unique_ptr<WebAccessibleResourcesInfo> ParseResourceStringList(
    const base::Value& entries,
    const Extension& extension,
    base::string16* error) {
  auto info = std::make_unique<WebAccessibleResourcesInfo>();
  URLPatternSet resource_set;
  int i = 0;
  for (const base::Value& value : entries.GetList()) {
    auto pattern = GetPatternOrError(value, extension, i, error);
    if (!pattern.has_value()) {
      return nullptr;
    }
    resource_set.AddPattern(pattern.value());
    ++i;
  }

  // In extensions where only a resource list is provided (as is the case in
  // manifest_version 2), resources are embeddable by any site. To handle
  // this, have |matches| match anything.
  URLPatternSet matches;

  matches.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, URLPattern::kAllUrlsPattern));
  info->web_accessible_resources.emplace_back(
      std::move(resource_set), std::move(matches), std::vector<ExtensionId>(),
      false);
  return info;
}

std::unique_ptr<WebAccessibleResourcesInfo> ParseEntryList(
    const base::Value& entries,
    const Extension& extension,
    base::string16* error) {
  auto info = std::make_unique<WebAccessibleResourcesInfo>();
  auto get_error = [](size_t i) {
    return ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidWebAccessibleResource, base::NumberToString(i));
  };

  int i = 0;
  for (const base::Value& value : entries.GetList()) {
    // Get and validate index element dictionary.
    if (!value.is_dict()) {
      *error = get_error(i);
      return nullptr;
    }

    // Compose dictionary from input.
    const base::Value* resources =
        value.FindKey(keys::kWebAccessibleResourcesResources);
    if (!resources || !resources->is_list()) {
      *error = get_error(i);
      return nullptr;
    }
    const base::Value* matches =
        value.FindKey(keys::kWebAccessibleResourcesMatches);
    if (matches && !matches->is_list()) {
      *error = get_error(i);
      return nullptr;
    }
    const base::Value* extension_ids =
        value.FindKey(keys::kWebAccessibleResourcesExtensionIds);
    if (extension_ids && !extension_ids->is_list()) {
      *error = get_error(i);
      return nullptr;
    }
    const base::Value* use_dynamic_url =
        value.FindKey(keys::kWebAccessibleResourcesUseDynamicUrl);
    if (use_dynamic_url && !use_dynamic_url->is_bool()) {
      *error = get_error(i);
      return nullptr;
    }

    // Entry must at least have resources, and one other valid key.
    if (!(matches || extension_ids || use_dynamic_url)) {
      *error = get_error(i);
      return nullptr;
    }

    // Prepare each key of the input dictionary.
    URLPatternSet resource_set;
    for (const auto& resource : resources->GetList()) {
      auto pattern = GetPatternOrError(resource, extension, i, error);
      if (!pattern.has_value()) {
        return nullptr;
      }
      resource_set.AddPattern(pattern.value());
    }
    URLPatternSet match_set;
    if (matches) {
      for (const auto& match : matches->GetList()) {
        URLPattern pattern(URLPattern::SCHEME_ALL);
        if (!match.is_string() || pattern.Parse(match.GetString()) !=
                                      URLPattern::ParseResult::kSuccess) {
          *error = get_error(i);
          return nullptr;
        }
        match_set.AddPattern(pattern);
      }
    }
    std::vector<ExtensionId> extension_id_list;
    if (extension_ids) {
      for (const auto& extension_id : extension_ids->GetList()) {
        if (!extension_id.is_string()) {
          *error = get_error(i);
          return nullptr;
        }
        const std::string& extension_id_str = extension_id.GetString();
        if (!crx_file::id_util::IdIsValid(extension_id_str)) {
          *error = get_error(i);
          return nullptr;
        }
        extension_id_list.emplace_back(extension_id_str);
      }
    }
    bool use_dynamic_url_bool = false;
    if (use_dynamic_url) {
      use_dynamic_url_bool = use_dynamic_url->GetBool();
    }

    info->web_accessible_resources.emplace_back(
        std::move(resource_set), std::move(match_set),
        std::move(extension_id_list), use_dynamic_url_bool);
    ++i;
  }
  return info;
}

}  // namespace

WebAccessibleResourcesInfo::WebAccessibleResourcesInfo() = default;

WebAccessibleResourcesInfo::~WebAccessibleResourcesInfo() = default;

// static
bool WebAccessibleResourcesInfo::IsResourceWebAccessible(
    const Extension* extension,
    const std::string& relative_path) {
  const WebAccessibleResourcesInfo* info = GetResourcesInfo(extension);
  if (!info) {  // No web-accessible resources
    return false;
  }
  for (const auto& entry : info->web_accessible_resources) {
    if (extension->ResourceMatches(entry.resources, relative_path)) {
      return true;
    }
  }
  return false;
}

// static
bool WebAccessibleResourcesInfo::HasWebAccessibleResources(
    const Extension* extension) {
  const WebAccessibleResourcesInfo* info = GetResourcesInfo(extension);
  return info && (info->web_accessible_resources.size() > 0);
}

WebAccessibleResourcesInfo::Entry::Entry() = default;

WebAccessibleResourcesInfo::Entry::Entry(
    WebAccessibleResourcesInfo::Entry&& rhs) = default;

WebAccessibleResourcesInfo::Entry::~Entry() = default;

WebAccessibleResourcesInfo::Entry::Entry(URLPatternSet resources,
                                         URLPatternSet matches,
                                         std::vector<ExtensionId> extension_ids,
                                         bool use_dynamic_url)
    : resources(std::move(resources)),
      matches(std::move(matches)),
      extension_ids(std::move(extension_ids)),
      use_dynamic_url(use_dynamic_url) {}

WebAccessibleResourcesHandler::WebAccessibleResourcesHandler() = default;

WebAccessibleResourcesHandler::~WebAccessibleResourcesHandler() {
}

bool WebAccessibleResourcesHandler::Parse(Extension* extension,
                                          base::string16* error) {
  const base::Value* entries = nullptr;
  if (!extension->manifest()->GetList(keys::kWebAccessibleResources,
                                      &entries)) {
    *error = base::ASCIIToUTF16(errors::kInvalidWebAccessibleResourcesList);
    return false;
  }
  auto info = extension->manifest_version() < 3
                  ? ParseResourceStringList(*entries, *extension, error)
                  : ParseEntryList(*entries, *extension, error);
  if (!info) {
    return false;
  }
  extension->SetManifestData(keys::kWebAccessibleResources, std::move(info));
  return true;
}
base::span<const char* const> WebAccessibleResourcesHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kWebAccessibleResources};
  return kKeys;
}

}  // namespace extensions
