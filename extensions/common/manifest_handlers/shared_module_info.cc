// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/shared_module_info.h"

#include <stddef.h>

#include <memory>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace keys = manifest_keys;
namespace values = manifest_values;
namespace errors = manifest_errors;

namespace {

const char kSharedModule[] = "shared_module";

static base::LazyInstance<SharedModuleInfo>::DestructorAtExit
    g_empty_shared_module_info = LAZY_INSTANCE_INITIALIZER;

const SharedModuleInfo& GetSharedModuleInfo(const Extension* extension) {
  SharedModuleInfo* info = static_cast<SharedModuleInfo*>(
      extension->GetManifestData(kSharedModule));
  if (!info)
    return g_empty_shared_module_info.Get();
  return *info;
}

}  // namespace

SharedModuleInfo::SharedModuleInfo() {
}

SharedModuleInfo::~SharedModuleInfo() {
}

// static
void SharedModuleInfo::ParseImportedPath(const std::string& path,
                                         std::string* import_id,
                                         std::string* import_relative_path) {
  std::vector<std::string> tokens = base::SplitString(
      path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.size() > 2 && tokens[0] == kModulesDir &&
      crx_file::id_util::IdIsValid(tokens[1])) {
    *import_id = tokens[1];
    *import_relative_path = tokens[2];
    for (size_t i = 3; i < tokens.size(); ++i)
      *import_relative_path += "/" + tokens[i];
  }
}

// static
bool SharedModuleInfo::IsImportedPath(const std::string& path) {
  std::vector<std::string> tokens = base::SplitString(
      path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.size() > 2 && tokens[0] == kModulesDir &&
      crx_file::id_util::IdIsValid(tokens[1])) {
    return true;
  }
  return false;
}

// static
bool SharedModuleInfo::IsSharedModule(const Extension* extension) {
  CHECK(extension);
  return extension->manifest()->is_shared_module();
}

// static
bool SharedModuleInfo::IsExportAllowedByAllowlist(const Extension* extension,
                                                  const std::string& other_id) {
  // Sanity check. In case the caller did not check |extension| to make sure it
  // is a shared module, we do not want it to appear that the extension with
  // |other_id| importing |extension| is valid.
  if (!SharedModuleInfo::IsSharedModule(extension))
    return false;
  const SharedModuleInfo& info = GetSharedModuleInfo(extension);
  if (info.export_allowlist_.empty())
    return true;
  if (info.export_allowlist_.find(other_id) != info.export_allowlist_.end())
    return true;
  return false;
}

// static
bool SharedModuleInfo::ImportsExtensionById(const Extension* extension,
                                            const std::string& other_id) {
  const SharedModuleInfo& info = GetSharedModuleInfo(extension);
  for (size_t i = 0; i < info.imports_.size(); i++) {
    if (info.imports_[i].extension_id == other_id)
      return true;
  }
  return false;
}

// static
bool SharedModuleInfo::ImportsModules(const Extension* extension) {
  return GetSharedModuleInfo(extension).imports_.size() > 0;
}

// static
const std::vector<SharedModuleInfo::ImportInfo>& SharedModuleInfo::GetImports(
    const Extension* extension) {
  return GetSharedModuleInfo(extension).imports_;
}

bool SharedModuleInfo::Parse(const Extension* extension,
                             base::string16* error) {
  bool has_import = extension->manifest()->HasKey(keys::kImport);
  bool has_export = extension->manifest()->HasKey(keys::kExport);
  if (!has_import && !has_export)
    return true;

  if (has_import && has_export) {
    *error = base::ASCIIToUTF16(errors::kInvalidImportAndExport);
    return false;
  }

  if (has_export) {
    const base::Value* export_value = nullptr;
    if (!extension->manifest()->GetDictionary(keys::kExport, &export_value)) {
      *error = base::ASCIIToUTF16(errors::kInvalidExport);
      return false;
    }

    // TODO(https://crbug.com/842354): Remove support for the legacy allowlist
    // key.
    const char* allowlist_key = nullptr;
    if (export_value->FindKey(keys::kSharedModuleAllowlist) != nullptr) {
      allowlist_key = keys::kSharedModuleAllowlist;
    } else if (export_value->FindKey(keys::kSharedModuleLegacyAllowlist) !=
               nullptr) {
      allowlist_key = keys::kSharedModuleLegacyAllowlist;
    }

    if (allowlist_key) {
      const base::Value* allowlist_value =
          export_value->FindKeyOfType(allowlist_key, base::Value::Type::LIST);
      if (allowlist_value == nullptr) {
        *error = base::ASCIIToUTF16(errors::kInvalidExportAllowlist);
        return false;
      }
      base::span<const base::Value> list_storage = allowlist_value->GetList();
      for (size_t i = 0; i < list_storage.size(); ++i) {
        if (!list_storage[i].is_string() ||
            !crx_file::id_util::IdIsValid(list_storage[i].GetString())) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidExportAllowlistString, base::NumberToString(i));
          return false;
        }
        export_allowlist_.insert(list_storage[i].GetString());
      }
    }
  }

  if (has_import) {
    const base::Value* import_list = nullptr;
    if (!extension->manifest()->GetList(keys::kImport, &import_list)) {
      *error = base::ASCIIToUTF16(errors::kInvalidImport);
      return false;
    }
    base::span<const base::Value> list_storage = import_list->GetList();
    for (size_t i = 0; i < list_storage.size(); ++i) {
      const base::Value& import_entry = list_storage[i];
      if (!import_entry.is_dict()) {
        *error = base::ASCIIToUTF16(errors::kInvalidImport);
        return false;
      }
      imports_.push_back(ImportInfo());
      const base::Value* extension_id =
          import_entry.FindKeyOfType(keys::kId, base::Value::Type::STRING);
      if (extension_id == nullptr ||
          !crx_file::id_util::IdIsValid(extension_id->GetString())) {
        *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidImportId,
                                                     base::NumberToString(i));
        return false;
      }
      imports_.back().extension_id = extension_id->GetString();
      const base::Value* min_version =
          import_entry.FindKey(keys::kMinimumVersion);
      if (min_version != nullptr) {
        if (!min_version->is_string()) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidImportVersion, base::NumberToString(i));
          return false;
        }
        imports_.back().minimum_version = min_version->GetString();
        base::Version v(min_version->GetString());
        if (!v.IsValid()) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidImportVersion, base::NumberToString(i));
          return false;
        }
      }
    }
  }
  return true;
}


SharedModuleHandler::SharedModuleHandler() {
}

SharedModuleHandler::~SharedModuleHandler() {
}

bool SharedModuleHandler::Parse(Extension* extension, base::string16* error) {
  std::unique_ptr<SharedModuleInfo> info(new SharedModuleInfo);
  if (!info->Parse(extension, error))
    return false;
  extension->SetManifestData(kSharedModule, std::move(info));
  return true;
}

bool SharedModuleHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  // Extensions that export resources should not have any permissions of their
  // own, instead they rely on the permissions of the extensions which import
  // them.
  if (SharedModuleInfo::IsSharedModule(extension) &&
      !extension->permissions_data()->active_permissions().IsEmpty()) {
    *error = errors::kInvalidExportPermissions;
    return false;
  }
  return true;
}

base::span<const char* const> SharedModuleHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kExport, keys::kImport};
  return kKeys;
}

}  // namespace extensions
