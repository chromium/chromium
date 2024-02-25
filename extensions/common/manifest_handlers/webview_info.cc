// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/webview_info.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = extensions::manifest_keys;
namespace errors = extensions::manifest_errors;

// A PartitionItem represents a set of accessible resources given a partition
// ID pattern.
class PartitionItem {
 public:
  explicit PartitionItem(const std::string& partition_pattern)
      : partition_pattern_(partition_pattern) {
  }

  virtual ~PartitionItem() {
  }

  bool Matches(const std::string& partition_id) const {
    return base::MatchPattern(partition_id, partition_pattern_);
  }

  // Adds a pattern to the set. Returns true if a new pattern was inserted,
  // false if the pattern was already in the set.
  bool AddPattern(const URLPattern& pattern) {
    return accessible_resources_.AddPattern(pattern);
  }

  const URLPatternSet& accessible_resources() const {
    return accessible_resources_;
  }
 private:
  // A pattern string that matches partition IDs.
  const std::string partition_pattern_;
  // A URL pattern set of resources accessible to the given
  // |partition_pattern_|.
  URLPatternSet accessible_resources_;
};

WebviewInfo::WebviewInfo(const ExtensionId& extension_id)
    : extension_id_(extension_id) {}

WebviewInfo::~WebviewInfo() {
}

// static
bool WebviewInfo::IsResourceWebviewAccessible(
    const Extension* extension,
    const std::string& partition_id,
    const std::string& relative_path) {
  if (!extension)
    return false;

  const WebviewInfo* webview_info = static_cast<const WebviewInfo*>(
      extension->GetManifestData(keys::kWebviewAccessibleResources));
  if (!webview_info)
    return false;

  for (const auto& item : webview_info->partition_items_) {
    if (item->Matches(partition_id) &&
        extension->ResourceMatches(item->accessible_resources(),
                                   relative_path)) {
      return true;
    }
  }

  return false;
}

// static
bool WebviewInfo::HasWebviewAccessibleResources(
    const Extension& extension,
    const std::string& partition_id) {
  const WebviewInfo* webview_info = static_cast<const WebviewInfo*>(
      extension.GetManifestData(keys::kWebviewAccessibleResources));
  if (!webview_info)
    return false;

  for (const auto& item : webview_info->partition_items_) {
    if (item->Matches(partition_id))
      return true;
  }
  return false;
}

void WebviewInfo::AddPartitionItem(std::unique_ptr<PartitionItem> item) {
  partition_items_.push_back(std::move(item));
}

WebviewHandler::WebviewHandler() = default;

WebviewHandler::~WebviewHandler() = default;

bool WebviewHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<WebviewInfo> info(new WebviewInfo(extension->id()));

  const base::Value::Dict* dict =
      extension->manifest()->available_values().FindDict(keys::kWebview);
  if (!dict) {
    *error = errors::kInvalidWebview;
    return false;
  }

  const base::Value::List* partition_list =
      dict->FindList(keys::kWebviewPartitions);
  if (partition_list == nullptr) {
    *error = errors::kInvalidWebviewPartitionsList;
    return false;
  }

  // The partition list must have at least one entry.
  if (partition_list->empty()) {
    *error = errors::kInvalidWebviewPartitionsList;
    return false;
  }

  for (size_t i = 0; i < partition_list->size(); ++i) {
    if (!(*partition_list)[i].is_dict()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebviewPartition, base::NumberToString(i));
      return false;
    }

    const base::Value::Dict& item_dict = (*partition_list)[i].GetDict();

    const std::string* partition_pattern =
        item_dict.FindString(keys::kWebviewName);
    if (partition_pattern == nullptr) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebviewPartitionName, base::NumberToString(i));
      return false;
    }

    const base::Value::List* url_list =
        item_dict.FindList(keys::kWebviewAccessibleResources);
    // The URL list should have at least one entry.
    if (url_list == nullptr || url_list->empty()) {
      *error = errors::kInvalidWebviewAccessibleResourcesList;
      return false;
    }

    auto partition_item = std::make_unique<PartitionItem>(*partition_pattern);

    for (const base::Value& item : *url_list) {
      if (!item.is_string()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidWebviewAccessibleResource, base::NumberToString(i));
        return false;
      }

      GURL pattern_url =
          Extension::GetResourceURL(extension->url(), item.GetString());
      // If passed a non-relative URL (like http://example.com),
      // Extension::GetResourceURL() will return that URL directly. (See
      // https://crbug.com/1135236). Check if this happened by comparing the
      // host.
      if (pattern_url.host_piece() != extension->id()) {
        // NOTE: Warning instead of error because there are existing apps that
        // have this bug, and we don't want to hard-error on them.
        // https://crbug.com/856948.
        std::string warning = ErrorUtils::FormatErrorMessage(
            errors::kInvalidWebviewAccessibleResource, base::NumberToString(i));
        extension->AddInstallWarning(
            InstallWarning(std::move(warning), keys::kWebview));
        continue;
      }
      URLPattern pattern(URLPattern::SCHEME_EXTENSION);
      if (pattern.Parse(pattern_url.spec()) !=
          URLPattern::ParseResult::kSuccess) {
        // NOTE: Warning instead of error because there are existing apps that
        // have this bug, and we don't want to hard-error on them.
        // https://crbug.com/856948.
        std::string warning = ErrorUtils::FormatErrorMessage(
            errors::kInvalidWebviewAccessibleResource, base::NumberToString(i));
        extension->AddInstallWarning(
            InstallWarning(std::move(warning), keys::kWebview));
        continue;
      }

      partition_item->AddPattern(std::move(pattern));
    }
    info->AddPartitionItem(std::move(partition_item));
  }

  extension->SetManifestData(keys::kWebviewAccessibleResources,
                             std::move(info));
  return true;
}

base::span<const char* const> WebviewHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kWebview};
  return kKeys;
}

}  // namespace extensions
