// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/file_handler_info.h"

#include <stddef.h>
#include <memory>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

const int kMaxTypeAndExtensionHandlers = 200;
const char kNotRecognized[] = "'%s' is not a recognized file handler property.";

bool IsSupportedVerb(const std::string& verb) {
  return verb == apps::file_handler_verbs::kOpenWith ||
         verb == apps::file_handler_verbs::kAddTo ||
         verb == apps::file_handler_verbs::kPackWith ||
         verb == apps::file_handler_verbs::kShareWith;
}

bool LoadFileHandler(const std::string& handler_id,
                     const base::Value::Dict& handler_info,
                     FileHandlersInfo* file_handlers,
                     std::u16string* error,
                     std::vector<InstallWarning>* install_warnings) {
  DCHECK(error);
  apps::FileHandlerInfo handler;

  handler.id = handler_id;

  const base::Value* mime_types = handler_info.Find(keys::kFileHandlerTypes);
  if (mime_types != nullptr && !mime_types->is_list()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlerType, handler_id);
    return false;
  }

  const base::Value* file_extensions =
      handler_info.Find(keys::kFileHandlerExtensions);
  if (file_extensions != nullptr && !file_extensions->is_list()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlerExtension, handler_id);
    return false;
  }

  handler.include_directories = false;
  const base::Value* include_directories =
      handler_info.Find(keys::kFileHandlerIncludeDirectories);
  if (include_directories != nullptr) {
    if (include_directories->is_bool()) {
      handler.include_directories = include_directories->GetBool();
    } else {
      *error = errors::kInvalidFileHandlerIncludeDirectories;
      return false;
    }
  }

  handler.verb = apps::file_handler_verbs::kOpenWith;
  const base::Value* file_handler = handler_info.Find(keys::kFileHandlerVerb);
  if (file_handler != nullptr) {
    if (file_handler->is_string() &&
        IsSupportedVerb(file_handler->GetString())) {
      handler.verb = file_handler->GetString();
    } else {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidFileHandlerVerb, handler_id);
      return false;
    }
  }

  if ((!mime_types || mime_types->GetList().empty()) &&
      (!file_extensions || file_extensions->GetList().empty()) &&
      !handler.include_directories) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlerNoTypeOrExtension,
        handler_id);
    return false;
  }

  if (mime_types) {
    const base::Value::List& list = mime_types->GetList();
    for (size_t i = 0; i < list.size(); ++i) {
      if (!list[i].is_string()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileHandlerTypeElement, handler_id,
            base::NumberToString(i));
        return false;
      }
      handler.types.insert(list[i].GetString());
    }
  }

  if (file_extensions) {
    const base::Value::List& list = file_extensions->GetList();
    for (size_t i = 0; i < list.size(); ++i) {
      if (!list[i].is_string()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileHandlerExtensionElement, handler_id,
            base::NumberToString(i));
        return false;
      }
      handler.extensions.insert(list[i].GetString());
    }
  }

  file_handlers->push_back(handler);

  // Check for unknown keys.
  for (auto entry : handler_info) {
    if (entry.first != keys::kFileHandlerExtensions &&
        entry.first != keys::kFileHandlerTypes &&
        entry.first != keys::kFileHandlerIncludeDirectories &&
        entry.first != keys::kFileHandlerVerb) {
      install_warnings->emplace_back(
          base::StringPrintf(kNotRecognized, entry.first.c_str()),
          keys::kFileHandlers, entry.first);
    }
  }

  return true;
}

}  // namespace

FileHandlerMatch::FileHandlerMatch() = default;
FileHandlerMatch::~FileHandlerMatch() = default;

FileHandlers::FileHandlers() = default;
FileHandlers::~FileHandlers() = default;

// static
const FileHandlersInfo* FileHandlers::GetFileHandlers(
    const Extension* extension) {
  CHECK(extension);
  if (WebFileHandlers::SupportsWebFileHandlers(*extension)) {
    return nullptr;
  }
  FileHandlers* info = static_cast<FileHandlers*>(
      extension->GetManifestData(keys::kFileHandlers));
  return info ? &info->file_handlers : nullptr;
}

FileHandlersParser::FileHandlersParser() = default;

FileHandlersParser::~FileHandlersParser() = default;

bool FileHandlersParser::Validate(const Extension* extension,
                                  std::string* error,
                                  std::vector<InstallWarning>* warnings) const {
  DCHECK(extension);

  // Web File Handlers.
  if (extension->manifest_version() >= 3) {
    return WebFileHandlersParser().Validate(extension, error, warnings);
  }

  return true;
}

bool FileHandlersParser::Parse(Extension* extension, std::u16string* error) {
  // If this is an MV3 extension, use the generated `file_handlers` object.
  if (extension->manifest_version() >= 3) {
    return WebFileHandlersParser().Parse(extension, error);
  }

  std::unique_ptr<FileHandlers> info(new FileHandlers);
  const base::Value::Dict* all_handlers =
      extension->manifest()->available_values().FindDict(keys::kFileHandlers);
  if (!all_handlers) {
    *error = errors::kInvalidFileHandlers;
    return false;
  }

  std::vector<InstallWarning> install_warnings;
  for (auto entry : *all_handlers) {
    if (!entry.second.is_dict()) {
      *error = errors::kInvalidFileHandlers;
      return false;
    }
    if (!LoadFileHandler(entry.first, entry.second.GetDict(),
                         &info->file_handlers, error, &install_warnings)) {
      return false;
    }
  }

  int filter_count = 0;
  for (const auto& iter : info->file_handlers)
    filter_count += iter.types.size() + iter.extensions.size();

  if (filter_count > kMaxTypeAndExtensionHandlers) {
    *error = errors::kInvalidFileHandlersTooManyTypesAndExtensions;
    return false;
  }

  extension->SetManifestData(keys::kFileHandlers, std::move(info));
  extension->AddInstallWarnings(std::move(install_warnings));
  return true;
}

base::span<const char* const> FileHandlersParser::Keys() const {
  static constexpr const char* kKeys[] = {keys::kFileHandlers};
  return kKeys;
}

}  // namespace extensions
