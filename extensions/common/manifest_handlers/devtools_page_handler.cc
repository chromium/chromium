// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/devtools_page_handler.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/api/chrome_url_overrides.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/api_permission.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace chrome_manifest_urls {
const GURL& GetDevToolsPage(const Extension* extension) {
  return ManifestURL::Get(extension, keys::kDevToolsPage);
}
}  // namespace chrome_manifest_urls

DevToolsPageHandler::DevToolsPageHandler() = default;
DevToolsPageHandler::~DevToolsPageHandler() = default;

bool DevToolsPageHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<ManifestURL> manifest_url(new ManifestURL);
  const std::string* devtools_str =
      extension->manifest()->FindStringPath(keys::kDevToolsPage);
  if (!devtools_str) {
    *error = errors::kInvalidDevToolsPage;
    return false;
  }
  GURL url = extension->GetResourceURL(*devtools_str);
  // SharedModuleInfo::IsImportedPath() does not require knowledge of data from
  // extension, so we can call it right here in Parse() and not Validate() and
  // do not need to specify DevToolsPageHandler::PrerequisiteKeys()
  const bool is_extension_url =
      url.is_valid() && !SharedModuleInfo::IsImportedPath(url.GetPath());
  if (!is_extension_url) {
    *error = errors::kInvalidDevToolsPage;
    return false;
  }
  manifest_url->url_ = std::move(url);
  extension->SetManifestData(keys::kDevToolsPage, std::move(manifest_url));
  PermissionsParser::AddAPIPermission(extension,
                                      mojom::APIPermissionID::kDevtools);
  return true;
}

base::span<const char* const> DevToolsPageHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kDevToolsPage};
  return kKeys;
}

bool DevToolsPageHandler::Validate(
    const Extension& extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  const GURL& url = chrome_manifest_urls::GetDevToolsPage(&extension);
  const base::FilePath relative_path =
      file_util::ExtensionURLToRelativeFilePath(url);
  const base::FilePath resource_path =
      extension.GetResource(relative_path).GetFilePath();
  if (resource_path.empty() || !base::PathExists(resource_path)) {
    const std::string message = ErrorUtils::FormatErrorMessage(
        errors::kFileNotFound, relative_path.AsUTF8Unsafe());
    warnings->emplace_back(message, keys::kDevToolsPage);
  }
  return true;
}

}  // namespace extensions
