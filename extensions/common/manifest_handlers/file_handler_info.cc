// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/file_handler_info.h"

#include <stddef.h>

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

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

}  // namespace

FileHandlerMatch::FileHandlerMatch() = default;
FileHandlerMatch::~FileHandlerMatch() = default;

FileHandlers::FileHandlers() {}
FileHandlers::~FileHandlers() {}

// static
const FileHandlersInfo* FileHandlers::GetFileHandlers(
    const Extension* extension) {
  FileHandlers* info = static_cast<FileHandlers*>(
      extension->GetManifestData(keys::kFileHandlers));
  return info ? &info->file_handlers : NULL;
}

FileHandlersParser::FileHandlersParser() {
}

FileHandlersParser::~FileHandlersParser() {
}

bool LoadFileHandler(const std::string& handler_id,
                     const base::Value& handler_info,
                     FileHandlersInfo* file_handlers,
                     std::u16string* error,
                     std::vector<InstallWarning>* install_warnings) {
  DCHECK(error);
  apps::FileHandlerInfo handler;

  handler.id = handler_id;

  const base::Value* mime_types = handler_info.FindKey(keys::kFileHandlerTypes);
  if (mime_types != nullptr && !mime_types->is_list()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlerType, handler_id);
    return false;
  }

  const base::Value* file_extensions =
      handler_info.FindKey(keys::kFileHandlerExtensions);
  if (file_extensions != nullptr && !file_extensions->is_list()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlerExtension, handler_id);
    return false;
  }

  handler.include_directories = false;
  const base::Value* include_directories =
      handler_info.FindKey(keys::kFileHandlerIncludeDirectories);
  if (include_directories != nullptr) {
    if (include_directories->is_bool()) {
      handler.include_directories = include_directories->GetBool();
    } else {
      *error = base::UTF8ToUTF16(errors::kInvalidFileHandlerIncludeDirectories);
      return false;
    }
  }

  handler.verb = apps::file_handler_verbs::kOpenWith;
  const base::Value* file_handler =
      handler_info.FindKey(keys::kFileHandlerVerb);
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
    base::Value::ConstListView list_storage = mime_types->GetList();
    for (size_t i = 0; i < list_storage.size(); ++i) {
      if (!list_storage[i].is_string()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileHandlerTypeElement, handler_id,
            base::NumberToString(i));
        return false;
      }
      handler.types.insert(list_storage[i].GetString());
    }
  }

  if (file_extensions) {
    base::Value::ConstListView list_storage = file_extensions->GetList();
    for (size_t i = 0; i < list_storage.size(); ++i) {
      if (!list_storage[i].is_string()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileHandlerExtensionElement, handler_id,
            base::NumberToString(i));
        return false;
      }
      handler.extensions.insert(list_storage[i].GetString());
    }
  }

  file_handlers->push_back(handler);

  // Check for unknown keys.
  for (auto entry : handler_info.DictItems()) {
    if (entry.first != keys::kFileHandlerExtensions &&
        entry.first != keys::kFileHandlerTypes &&
        entry.first != keys::kFileHandlerIncludeDirectories &&
        entry.first != keys::kFileHandlerVerb) {
      install_warnings->push_back(InstallWarning(
          base::StringPrintf(kNotRecognized, entry.first.c_str()),
          keys::kFileHandlers, entry.first));
    }
  }

  return true;
}

bool FileHandlersParser::Parse(Extension* extension, std::u16string* error) {
  // Don't load file handlers for hosted_apps unless they're also bookmark apps.
  // This check can be removed when bookmark apps are migrated off hosted apps,
  // and hosted_apps should be removed from the list of valid extension types
  // for "file_handling" in extensions/common/api/_manifest_features.json.
  if (extension->is_hosted_app() && !extension->from_bookmark()) {
    extension->AddInstallWarning(
        InstallWarning(errors::kInvalidFileHandlersHostedAppsNotSupported,
                       keys::kFileHandlers));
    return true;
  }

  std::unique_ptr<FileHandlers> info(new FileHandlers);
  const base::Value* all_handlers = nullptr;
  if (!extension->manifest()->GetDictionary(keys::kFileHandlers,
                                            &all_handlers)) {
    *error = base::ASCIIToUTF16(errors::kInvalidFileHandlers);
    return false;
  }

  std::vector<InstallWarning> install_warnings;
  for (auto entry : all_handlers->DictItems()) {
    if (!entry.second.is_dict()) {
      *error = base::ASCIIToUTF16(errors::kInvalidFileHandlers);
      return false;
    }
    if (!LoadFileHandler(entry.first, entry.second, &info->file_handlers, error,
                         &install_warnings)) {
      return false;
    }
  }

  int filter_count = 0;
  for (FileHandlersInfo::const_iterator iter = info->file_handlers.begin();
       iter != info->file_handlers.end();
       iter++) {
    filter_count += iter->types.size();
    filter_count += iter->extensions.size();
  }

  if (filter_count > kMaxTypeAndExtensionHandlers) {
    *error = base::ASCIIToUTF16(
        errors::kInvalidFileHandlersTooManyTypesAndExtensions);
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
