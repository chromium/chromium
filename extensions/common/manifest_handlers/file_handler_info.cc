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

namespace file_handler_verbs {

const char kOpenWith[] = "open_with";
const char kAddTo[] = "add_to";
const char kPackWith[] = "pack_with";
const char kShareWith[] = "share_with";

}  // namespace file_handler_verbs

namespace {

const int kMaxTypeAndExtensionHandlers = 200;
const char kNotRecognized[] = "'%s' is not a recognized file handler property.";

bool IsSupportedVerb(const std::string& verb) {
  return verb == file_handler_verbs::kOpenWith ||
         verb == file_handler_verbs::kAddTo ||
         verb == file_handler_verbs::kPackWith ||
         verb == file_handler_verbs::kShareWith;
}

}  // namespace

FileHandlerInfo::FileHandlerInfo()
    : include_directories(false), verb(file_handler_verbs::kOpenWith) {}
FileHandlerInfo::FileHandlerInfo(const FileHandlerInfo& other) = default;
FileHandlerInfo::~FileHandlerInfo() {}

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
                     const base::DictionaryValue& handler_info,
                     FileHandlersInfo* file_handlers,
                     base::string16* error,
                     std::vector<InstallWarning>* install_warnings) {
  DCHECK(error);
  FileHandlerInfo handler;

  handler.id = handler_id;

  const base::ListValue* mime_types = NULL;
  if (handler_info.HasKey(keys::kFileHandlerTypes) &&
      !handler_info.GetList(keys::kFileHandlerTypes, &mime_types)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlerType, handler_id);
    return false;
  }

  const base::ListValue* file_extensions = NULL;
  if (handler_info.HasKey(keys::kFileHandlerExtensions) &&
      !handler_info.GetList(keys::kFileHandlerExtensions, &file_extensions)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlerExtension, handler_id);
    return false;
  }

  handler.include_directories = false;
  if (handler_info.HasKey(keys::kFileHandlerIncludeDirectories) &&
      !handler_info.GetBoolean(keys::kFileHandlerIncludeDirectories,
                               &handler.include_directories)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlerIncludeDirectories, handler_id);
    return false;
  }

  handler.verb = file_handler_verbs::kOpenWith;
  if (handler_info.HasKey(keys::kFileHandlerVerb) &&
      (!handler_info.GetString(keys::kFileHandlerVerb, &handler.verb) ||
       !IsSupportedVerb(handler.verb))) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlerVerb, handler_id);
    return false;
  }

  if ((!mime_types || mime_types->empty()) &&
      (!file_extensions || file_extensions->empty()) &&
      !handler.include_directories) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidFileHandlerNoTypeOrExtension,
        handler_id);
    return false;
  }

  if (mime_types) {
    std::string type;
    for (size_t i = 0; i < mime_types->GetSize(); ++i) {
      if (!mime_types->GetString(i, &type)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileHandlerTypeElement, handler_id,
            base::NumberToString(i));
        return false;
      }
      handler.types.insert(type);
    }
  }

  if (file_extensions) {
    std::string file_extension;
    for (size_t i = 0; i < file_extensions->GetSize(); ++i) {
      if (!file_extensions->GetString(i, &file_extension)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidFileHandlerExtensionElement, handler_id,
            base::NumberToString(i));
        return false;
      }
      handler.extensions.insert(file_extension);
    }
  }

  file_handlers->push_back(handler);

  // Check for unknown keys.
  for (base::DictionaryValue::Iterator it(handler_info); !it.IsAtEnd();
       it.Advance()) {
    if (it.key() != keys::kFileHandlerExtensions &&
        it.key() != keys::kFileHandlerTypes &&
        it.key() != keys::kFileHandlerIncludeDirectories &&
        it.key() != keys::kFileHandlerVerb) {
      install_warnings->push_back(
          InstallWarning(base::StringPrintf(kNotRecognized, it.key().c_str()),
                         keys::kFileHandlers,
                         it.key()));
    }
  }

  return true;
}

bool FileHandlersParser::Parse(Extension* extension, base::string16* error) {
  std::unique_ptr<FileHandlers> info(new FileHandlers);
  const base::DictionaryValue* all_handlers = NULL;
  if (!extension->manifest()->GetDictionary(keys::kFileHandlers,
                                            &all_handlers)) {
    *error = base::ASCIIToUTF16(errors::kInvalidFileHandlers);
    return false;
  }

  std::vector<InstallWarning> install_warnings;
  for (base::DictionaryValue::Iterator iter(*all_handlers);
       !iter.IsAtEnd();
       iter.Advance()) {
    const base::DictionaryValue* handler = NULL;
    if (iter.value().GetAsDictionary(&handler)) {
      if (!LoadFileHandler(iter.key(),
                           *handler,
                           &info->file_handlers,
                           error,
                           &install_warnings))
        return false;
    } else {
      *error = base::ASCIIToUTF16(errors::kInvalidFileHandlers);
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
