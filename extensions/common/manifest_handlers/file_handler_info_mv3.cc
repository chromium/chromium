// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/file_handler_info_mv3.h"

#include "extensions/common/error_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace errors = manifest_errors;

namespace {

using FileHandlersManifestKeys = api::file_handlers::ManifestKeys;

std::unique_ptr<FileHandlersMV3> ParseFromList(const Extension& extension,
                                               std::u16string* error) {
  FileHandlersManifestKeys manifest_keys;
  if (!FileHandlersManifestKeys::ParseFromDictionary(
          extension.manifest()->available_values().GetDict(), &manifest_keys,
          error)) {
    return nullptr;
  }

  auto get_error = [](size_t i, base::StringPiece message) {
    return ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlersMV3, base::NumberToString(i), message);
  };

  auto info = std::make_unique<FileHandlersMV3>();

  // file_handlers: array. can't be empty
  if (manifest_keys.file_handlers.size() == 0) {
    *error = get_error(0, "At least one File Handler must be present.");
    return nullptr;
  }

  for (size_t i = 0; i < manifest_keys.file_handlers.size(); i++) {
    auto& file_handler = manifest_keys.file_handlers[i];

    // name: string. Can't be empty.
    if (file_handler.name.size() == 0) {
      *error = get_error(i, "`name` must have a value.");
      return nullptr;
    }

    // action: string. Can't be empty. Starts with slash.
    if (file_handler.action.size() == 0) {
      *error = get_error(i, "`action` must have a value.");
      return nullptr;
    } else if (file_handler.action[0] != '/') {
      *error = get_error(i, "`action` must start with a forward slash.");
      return nullptr;
    }

    info->file_handlers.emplace_back(std::move(file_handler));
  }
  return info;
}

}  // namespace

FileHandlersMV3::FileHandlersMV3() = default;
FileHandlersMV3::~FileHandlersMV3() = default;

FileHandlersParserMV3::FileHandlersParserMV3() = default;
FileHandlersParserMV3::~FileHandlersParserMV3() = default;

bool FileHandlersParserMV3::Parse(Extension* extension, std::u16string* error) {
  auto info = ParseFromList(*extension, error);
  if (!info)
    return false;
  extension->SetManifestData(FileHandlersManifestKeys::kFileHandlers,
                             std::move(info));
  return true;
}

base::span<const char* const> FileHandlersParserMV3::Keys() const {
  static constexpr const char* kKeys[] = {
      FileHandlersManifestKeys::kFileHandlers};
  return kKeys;
}

bool FileHandlersParserMV3::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  // TODO(1313786): Validate that icons exist.
  return true;
}

}  // namespace extensions
