// Copyright 2014 The Chromium Authors. All rights reserved.
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

WebviewInfo::WebviewInfo(const std::string& extension_id)
    : extension_id_(extension_id) {
}

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

WebviewHandler::WebviewHandler() {
}

WebviewHandler::~WebviewHandler() {
}

bool WebviewHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<WebviewInfo> info(new WebviewInfo(extension->id()));

  const base::Value* dict_value = nullptr;
  if (!extension->manifest()->GetDictionary(keys::kWebview,
                                            &dict_value)) {
    *error = base::ASCIIToUTF16(errors::kInvalidWebview);
    return false;
  }

  const base::Value* partition_list = dict_value->FindKeyOfType(
      keys::kWebviewPartitions, base::Value::Type::LIST);
  if (partition_list == nullptr) {
    *error = base::ASCIIToUTF16(errors::kInvalidWebviewPartitionsList);
    return false;
  }

  // The partition list must have at least one entry.
  base::Value::ConstListView partition_list_view = partition_list->GetList();
  if (partition_list_view.empty()) {
    *error = base::ASCIIToUTF16(errors::kInvalidWebviewPartitionsList);
    return false;
  }

  for (size_t i = 0; i < partition_list_view.size(); ++i) {
    if (!partition_list_view[i].is_dict()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebviewPartition, base::NumberToString(i));
      return false;
    }

    const base::Value* webview_name = partition_list_view[i].FindKeyOfType(
        keys::kWebviewName, base::Value::Type::STRING);
    if (webview_name == nullptr) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebviewPartitionName, base::NumberToString(i));
      return false;
    }
    const std::string& partition_pattern = webview_name->GetString();

    const base::Value* url_list = partition_list_view[i].FindKeyOfType(
        keys::kWebviewAccessibleResources, base::Value::Type::LIST);
    if (url_list == nullptr) {
      *error = base::ASCIIToUTF16(
          errors::kInvalidWebviewAccessibleResourcesList);
      return false;
    }

    // The URL list should have at least one entry.
    base::Value::ConstListView url_list_view = url_list->GetList();
    if (url_list_view.empty()) {
      *error = base::ASCIIToUTF16(
          errors::kInvalidWebviewAccessibleResourcesList);
      return false;
    }

    auto partition_item = std::make_unique<PartitionItem>(partition_pattern);

    for (size_t i = 0; i < url_list_view.size(); ++i) {
      if (!url_list_view[i].is_string()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidWebviewAccessibleResource, base::NumberToString(i));
        return false;
      }

      GURL pattern_url = Extension::GetResourceURL(
          extension->url(), url_list_view[i].GetString());
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
