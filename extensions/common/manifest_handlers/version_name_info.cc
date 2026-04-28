// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/version_name_info.h"

#include "base/values.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

VersionNameInfo::VersionNameInfo(const std::string& version_name)
    : version_name_(version_name) {}

VersionNameInfo::~VersionNameInfo() = default;

// static
const std::string& VersionNameInfo::GetVersionName(const Extension& extension) {
  const VersionNameInfo* info = static_cast<const VersionNameInfo*>(
      extension.GetManifestData(manifest_keys::kVersionName));
  return info ? info->version_name_ : base::EmptyString();
}

VersionNameHandler::VersionNameHandler() = default;
VersionNameHandler::~VersionNameHandler() = default;

bool VersionNameHandler::Parse(Extension* extension, std::u16string* error) {
  const base::Value* version_name_value =
      extension->manifest()->FindKey(manifest_keys::kVersionName);
  CHECK(version_name_value);
  if (!version_name_value->is_string()) {
    *error = manifest_errors::kInvalidVersionName;
    return false;
  }

  const std::string& version_name = version_name_value->GetString();
  // If "version_name" is empty, we do not need to save it at all.
  if (!version_name.empty()) {
    extension->SetManifestData(manifest_keys::kVersionName,
                               std::make_unique<VersionNameInfo>(version_name));
  }

  return true;
}

base::span<const char* const> VersionNameHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kVersionName};
  return kKeys;
}

}  // namespace extensions
