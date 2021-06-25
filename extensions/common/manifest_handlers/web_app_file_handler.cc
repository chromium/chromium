// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/web_app_file_handler.h"

#include <stddef.h>

#include <memory>

#include "base/containers/flat_set.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

absl::optional<base::flat_set<std::string>> LoadFileExtensions(
    const base::Value& entry,
    int* error_index) {
  auto extract_file_extension =
      [](const base::Value& entry) -> absl::optional<std::string> {
    if (!entry.is_string())
      return absl::nullopt;
    std::string file_extension = entry.GetString();
    if (file_extension.empty() || file_extension[0] != '.')
      return absl::nullopt;
    return file_extension;
  };

  // An accept entry can validly map from a MIME type to either a single file
  // extension (a string), or alist of file extensions.
  if (entry.is_string()) {
    absl::optional<std::string> file_extension = extract_file_extension(entry);
    if (!file_extension) {
      *error_index = 0;
      return absl::nullopt;
    }
    return base::flat_set<std::string>{*file_extension};
  }

  DCHECK(entry.is_list());
  base::flat_set<std::string> file_extensions;
  base::Value::ConstListView entry_list = entry.GetList();
  for (size_t i = 0; i < entry_list.size(); i++) {
    absl::optional<std::string> file_extension =
        extract_file_extension(entry_list[i]);
    if (!file_extension) {
      *error_index = i;
      return absl::nullopt;
    }
    file_extensions.insert(*file_extension);
  }
  return file_extensions;
}

bool LoadWebAppFileHandler(const std::string& manifest_entry_index,
                           const base::Value& manifest_entry,
                           apps::FileHandlers* file_handlers,
                           std::u16string* error,
                           std::vector<InstallWarning>* install_warnings) {
  DCHECK(error);

  const base::Value* action = manifest_entry.FindKeyOfType(
      keys::kWebAppFileHandlerAction, base::Value::Type::STRING);
  if (!action) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidWebAppFileHandlerAction, manifest_entry_index);
    return false;
  }

  apps::FileHandler file_handler;
  file_handler.action = GURL(action->GetString());

  if (!file_handler.action.is_valid()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidWebAppFileHandlerAction, manifest_entry_index);
    return false;
  }

  const base::Value* accept = manifest_entry.FindKeyOfType(
      keys::kWebAppFileHandlerAccept, base::Value::Type::DICTIONARY);
  if (!accept) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidWebAppFileHandlerAccept, manifest_entry_index);
    return false;
  }

  if (accept->DictEmpty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidWebAppFileHandlerEmptyAccept, manifest_entry_index);
    return false;
  }

  for (auto manifest_accept_entry : accept->DictItems()) {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = manifest_accept_entry.first;

    if (!manifest_accept_entry.second.is_string() &&
        !manifest_accept_entry.second.is_list()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebAppFileHandlerFileExtensions, manifest_entry_index,
          accept_entry.mime_type);
      return false;
    }

    int error_index = -1;
    absl::optional<base::flat_set<std::string>> file_extensions =
        LoadFileExtensions(manifest_accept_entry.second, &error_index);
    if (!file_extensions) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebAppFileHandlerFileExtension, manifest_entry_index,
          accept_entry.mime_type, base::NumberToString(error_index));
      return false;
    }

    accept_entry.file_extensions = std::move(*file_extensions);
    file_handler.accept.push_back(std::move(accept_entry));
  }

  file_handlers->push_back(std::move(file_handler));
  return true;
}

}  // namespace

WebAppFileHandlers::WebAppFileHandlers() = default;
WebAppFileHandlers::~WebAppFileHandlers() = default;

// static
const apps::FileHandlers* WebAppFileHandlers::GetWebAppFileHandlers(
    const Extension* extension) {
  if (!extension)
    return nullptr;
  WebAppFileHandlers* manifest_data = static_cast<WebAppFileHandlers*>(
      extension->GetManifestData(keys::kWebAppFileHandlers));
  return manifest_data ? &manifest_data->file_handlers : nullptr;
}

WebAppFileHandlersParser::WebAppFileHandlersParser() = default;
WebAppFileHandlersParser::~WebAppFileHandlersParser() = default;

bool WebAppFileHandlersParser::Parse(Extension* extension,
                                     std::u16string* error) {
  // The "web_app_file_handlers" key is only available for Bookmark Apps.
  // Including it elsewhere results in an install warning, and the file handlers
  // are not parsed.
  if (!extension->from_bookmark()) {
    extension->AddInstallWarning(
        InstallWarning(errors::kInvalidWebAppFileHandlersNotBookmarkApp));
    return true;
  }

  std::unique_ptr<WebAppFileHandlers> manifest_data =
      std::make_unique<WebAppFileHandlers>();

  const base::Value* file_handlers = nullptr;
  if (!extension->manifest()->GetList(keys::kWebAppFileHandlers,
                                      &file_handlers)) {
    *error = base::ASCIIToUTF16(errors::kInvalidWebAppFileHandlers);
    return false;
  }

  std::vector<InstallWarning> install_warnings;

  base::Value::ConstListView file_handlers_list = file_handlers->GetList();
  for (size_t i = 0; i < file_handlers_list.size(); i++) {
    std::string manifest_entry_index = base::NumberToString(i);
    if (!file_handlers_list[i].is_dict()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebAppFileHandler, manifest_entry_index);
      return false;
    }
    if (!LoadWebAppFileHandler(manifest_entry_index, file_handlers_list[i],
                               &manifest_data->file_handlers, error,
                               &install_warnings)) {
      return false;
    }
  }

  extension->SetManifestData(keys::kWebAppFileHandlers,
                             std::move(manifest_data));
  extension->AddInstallWarnings(std::move(install_warnings));
  return true;
}

base::span<const char* const> WebAppFileHandlersParser::Keys() const {
  static constexpr const char* kKeys[] = {keys::kWebAppFileHandlers};
  return kKeys;
}

}  // namespace extensions
