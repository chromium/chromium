// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/web_file_handlers_info.h"

#include "base/strings/string_split.h"
#include "extensions/common/api/file_handlers.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

using FileHandlersManifestKeys = api::file_handlers::ManifestKeys;

// Verifies manifest input. Disambiguates `file_extensions` on `accept` into a
// list, which could otherwise have also been a string. `icon.sizes` remains as
// is because the generated data type only accepts a string. This string can be
// parsed with a method that gets a list of sizes.
// TODO(crbug/1179530): Re-use Blink parser.
std::unique_ptr<WebFileHandlers> ParseFromList(const Extension& extension,
                                               std::u16string* error) {
  FileHandlersManifestKeys manifest_keys;
  if (!FileHandlersManifestKeys::ParseFromDictionary(
          extension.manifest()->available_values(), manifest_keys, *error)) {
    return nullptr;
  }

  auto get_error = [](size_t i, base::StringPiece message) {
    return ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidWebFileHandlers, base::NumberToString(i),
        message);
  };

  auto info = std::make_unique<WebFileHandlers>();

  // file_handlers: array. can't be empty
  if (manifest_keys.file_handlers.empty()) {
    *error = get_error(0, "At least one File Handler must be present.");
    return nullptr;
  }

  for (size_t i = 0; i < manifest_keys.file_handlers.size(); i++) {
    FileHandler file_handler;
    auto& manifest_file_handler = manifest_keys.file_handlers[i];

    // `name` is a string that can't be empty.
    if (manifest_file_handler.name.empty()) {
      *error = get_error(i, "`name` must have a value.");
      return nullptr;
    }
    file_handler.name = std::move(manifest_file_handler.name);

    // `action` is a string that can't be empty and starts with slash.
    if (manifest_file_handler.action.empty()) {
      *error = get_error(i, "`action` must have a value.");
      return nullptr;
    } else if (manifest_file_handler.action[0] != '/') {
      *error = get_error(i, "`action` must start with a forward slash.");
      return nullptr;
    }
    file_handler.action = std::move(manifest_file_handler.action);

    // `accept` is a dictionary. MIME types are strings with one slash. File
    // extensions are strings or an array of strings where each string has a
    // leading period.
    if (manifest_file_handler.accept.additional_properties.empty()) {
      *error = get_error(i, "`accept` cannot be empty.");
      return nullptr;
    }

    // Mime type keyed by string or array of strings of file extensions.
    base::Value::Dict accept;
    for (const auto [mime_type, file_extensions] :
         manifest_file_handler.accept.additional_properties) {
      // Verify that mime type only has one slash.
      // TODO(crbug/1179530): Verify that slash isn't the first or last char.
      // TODO(crbug/1179530): Cross-check slash against canonical mime list.
      auto num_slashes = std::count(mime_type.begin(), mime_type.end(), '/');
      if (num_slashes != 1) {
        *error =
            get_error(i, "`accept` mime type must have exactly one slash.");
        return nullptr;
      }

      // Verify that file extension has a leading dot.
      base::Value::List file_extension_list;
      if (file_extensions.is_string()) {
        file_extension_list.Append(file_extensions.GetString());
      } else if (file_extensions.is_list()) {
        file_extension_list = file_extensions.GetList().Clone();
      } else {
        *error = get_error(i, "`accept` must have a valid file extension.");
        return nullptr;
      }
      if (file_extension_list.empty()) {
        *error = get_error(i, "`accept` file extension must have a value.");
        return nullptr;
      }

      // Verify file extensions in `accept`.
      for (const auto& file_extension : file_extension_list) {
        auto file_extension_item = file_extension.GetString();
        if (file_extension_item.empty()) {
          *error = get_error(i, "`accept` file extension must have a value.");
          return nullptr;
        }
        if (file_extension_item[0] != '.') {
          *error = get_error(
              i, "`accept` file extension must have a leading period.");
          return nullptr;
        }
      }

      // TODO(crbug/1179530): Error if there are duplicate mime_types.
      accept.Set(mime_type, std::move(file_extension_list));
    }

    // Make the temporary `accept` permanent by assigning to `file_handler`.
    api::file_handlers::FileHandler::Accept::Populate(
        accept, file_handler.accept, *error);

    // `icon` is an optional array of dictionaries.
    if (manifest_file_handler.icons.has_value()) {
      for (const auto& icon : manifest_file_handler.icons.value()) {
        if (icon.src.empty()) {
          *error = get_error(i, "`icon.src` must have a value.");
          return nullptr;
        }
        if (icon.sizes.has_value()) {
          auto& sizes = icon.sizes.value();
          if (sizes.empty()) {
            *error = get_error(i, "`icon.sizes` must have a value.");
            return nullptr;
          }
          auto size_list =
              base::SplitString(sizes, " ", base::TRIM_WHITESPACE,
                                base::SplitResult::SPLIT_WANT_NONEMPTY);
          for (const auto& size : size_list) {
            auto dimensions =
                base::SplitString(size, "x", base::TRIM_WHITESPACE,
                                  base::SplitResult::SPLIT_WANT_NONEMPTY);
            if (dimensions.size() != 2) {
              *error = get_error(i, "`icon.sizes` must have width and height.");
              return nullptr;
            }
            for (const auto& dimension : dimensions) {
              int parsed_dimension = 0;
              if (!base::StringToInt(dimension, &parsed_dimension)) {
                *error =
                    get_error(i, "`icon.sizes` dimensions must be digits.");
                return nullptr;
              }
            }
          }
        }
      }

      // Append icon.
      file_handler.icons = std::move(manifest_file_handler.icons);
    }

    // Append file handlers.
    info->file_handlers.emplace_back(std::move(file_handler));
  }

  return info;
}

}  // namespace

WebFileHandlers::WebFileHandlers() = default;
WebFileHandlers::~WebFileHandlers() = default;

// static
bool WebFileHandlers::HasFileHandlers(const Extension& extension) {
  const WebFileHandlersInfo* info = GetFileHandlers(extension);
  return info && info->size() > 0;
}

// static
const WebFileHandlersInfo* WebFileHandlers::GetFileHandlers(
    const Extension& extension) {
  // Guard against incompatible extension manifest versions.
  if (!WebFileHandlers::SupportsWebFileHandlers(extension.manifest_version())) {
    return nullptr;
  }

  WebFileHandlers* info = static_cast<WebFileHandlers*>(
      extension.GetManifestData(manifest_keys::kFileHandlers));
  return info ? &info->file_handlers : nullptr;
}

WebFileHandlersParser::WebFileHandlersParser() = default;
WebFileHandlersParser::~WebFileHandlersParser() = default;

bool WebFileHandlersParser::Parse(Extension* extension, std::u16string* error) {
  // Guard against incompatible extension manifest versions.
  DCHECK(extension);
  DCHECK(
      WebFileHandlers::SupportsWebFileHandlers(extension->manifest_version()));

  auto info = ParseFromList(*extension, error);
  if (!info) {
    return false;
  }

  extension->SetManifestData(FileHandlersManifestKeys::kFileHandlers,
                             std::move(info));
  return true;
}

base::span<const char* const> WebFileHandlersParser::Keys() const {
  static constexpr const char* kKeys[] = {
      FileHandlersManifestKeys::kFileHandlers};
  return kKeys;
}

bool WebFileHandlersParser::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  // TODO(1313786): Verify that icons exist.
  return true;
}

bool WebFileHandlers::SupportsWebFileHandlers(const int manifest_version) {
  return manifest_version >= 3 &&
         base::FeatureList::IsEnabled(
             extensions_features::kExtensionWebFileHandlers);
}

}  // namespace extensions
