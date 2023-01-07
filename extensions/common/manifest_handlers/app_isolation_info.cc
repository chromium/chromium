// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/app_isolation_info.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission_set.h"

namespace extensions {

namespace keys = manifest_keys;

AppIsolationInfo::AppIsolationInfo(bool isolated_storage)
    : has_isolated_storage(isolated_storage) {
}

AppIsolationInfo::~AppIsolationInfo() {
}

// static
bool AppIsolationInfo::HasIsolatedStorage(const Extension* extension) {
  AppIsolationInfo* info = static_cast<AppIsolationInfo*>(
      extension->GetManifestData(keys::kIsolation));
  return info ? info->has_isolated_storage : false;
}

AppIsolationHandler::AppIsolationHandler() {
}

AppIsolationHandler::~AppIsolationHandler() {
}

bool AppIsolationHandler::Parse(Extension* extension, std::u16string* error) {
  // Platform apps always get isolated storage.
  if (extension->is_platform_app()) {
    extension->SetManifestData(keys::kIsolation,
                               std::make_unique<AppIsolationInfo>(true));
    return true;
  }

  // Other apps only get it if it is requested _and_ experimental APIs are
  // enabled.
  if (!extension->is_app() ||
      !PermissionsParser::HasAPIPermission(
          extension, mojom::APIPermissionID::kExperimental)) {
    return true;
  }

  // We should only be parsing if the extension has the key in the manifest,
  // or is a platform app (which we already handled).
  DCHECK(extension->manifest()->FindPath(keys::kIsolation));

  const base::Value* isolation_list = nullptr;
  if (!extension->manifest()->GetList(keys::kIsolation, &isolation_list)) {
    *error = manifest_errors::kInvalidIsolation;
    return false;
  }

  bool has_isolated_storage = false;
  const base::Value::List& list = isolation_list->GetList();
  for (size_t i = 0; i < list.size(); ++i) {
    if (!list[i].is_string()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          manifest_errors::kInvalidIsolationValue, base::NumberToString(i));
      return false;
    }

    const std::string& isolation_string = list[i].GetString();
    // Check for isolated storage.
    if (isolation_string == manifest_values::kIsolatedStorage) {
      has_isolated_storage = true;
    } else {
      DLOG(WARNING) << "Did not recognize isolation type: " << isolation_string;
    }
  }

  if (has_isolated_storage)
    extension->SetManifestData(keys::kIsolation,
                               std::make_unique<AppIsolationInfo>(true));

  return true;
}

bool AppIsolationHandler::AlwaysParseForType(Manifest::Type type) const {
  return type == Manifest::TYPE_PLATFORM_APP;
}

base::span<const char* const> AppIsolationHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kIsolation};
  return kKeys;
}

}  // namespace extensions
