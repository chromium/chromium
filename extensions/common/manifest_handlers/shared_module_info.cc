// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/shared_module_info.h"

#include <stddef.h>

#include <iterator>
#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/api/shared_module.h"
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

using ManifestKeys = api::shared_module::ManifestKeys;

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

SharedModuleHandler::SharedModuleHandler() = default;
SharedModuleHandler::~SharedModuleHandler() = default;

bool SharedModuleHandler::Parse(Extension* extension, std::u16string* error) {
  ManifestKeys manifest_keys;
  if (!ManifestKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys, *error)) {
    return false;
  }

  bool has_import = !!manifest_keys.import;
  bool has_export = !!manifest_keys.export_;
  DCHECK(has_import || has_export);

  auto info = std::make_unique<SharedModuleInfo>();

  if (has_import && has_export) {
    *error = errors::kInvalidImportAndExport;
    return false;
  }

  if (has_export && manifest_keys.export_->allowlist) {
    auto begin = manifest_keys.export_->allowlist->begin();
    auto end = manifest_keys.export_->allowlist->end();
    auto it = base::ranges::find_if_not(*manifest_keys.export_->allowlist,
                                        &crx_file::id_util::IdIsValid);
    if (it != end) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidExportAllowlistString,
          base::NumberToString(it - begin));
      return false;
    }
    info->set_export_allowlist(std::set<std::string>(
        std::make_move_iterator(begin), std::make_move_iterator(end)));
  }

  if (has_import) {
    std::vector<SharedModuleInfo::ImportInfo> imports;
    imports.reserve(manifest_keys.import->size());
    for (size_t i = 0; i < manifest_keys.import->size(); i++) {
      auto& import = manifest_keys.import->at(i);
      if (!crx_file::id_util::IdIsValid(import.id)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidImportId,
                                                     base::NumberToString(i));
        return false;
      }

      SharedModuleInfo::ImportInfo import_info;
      import_info.extension_id = std::move(import.id);

      if (import.minimum_version) {
        base::Version v(*import.minimum_version);
        if (!v.IsValid()) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidImportVersion, base::NumberToString(i));
          return false;
        }
        import_info.minimum_version = std::move(*import.minimum_version);
      }
      imports.push_back(std::move(import_info));
    }
    info->set_imports(std::move(imports));
  }

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
  static constexpr const char* kKeys[] = {ManifestKeys::kImport,
                                          ManifestKeys::kExport};
  return kKeys;
}

}  // namespace extensions
