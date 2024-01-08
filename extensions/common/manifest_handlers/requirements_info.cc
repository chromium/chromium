// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/requirements_info.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/api/requirements.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace errors = manifest_errors;

using ManifestKeys = api::requirements::ManifestKeys;

RequirementsInfo::RequirementsInfo() = default;
RequirementsInfo::~RequirementsInfo() = default;

// static
const RequirementsInfo& RequirementsInfo::GetRequirements(
    const Extension* extension) {
  RequirementsInfo* info = static_cast<RequirementsInfo*>(
      extension->GetManifestData(ManifestKeys::kRequirements));

  // We should be guaranteed to have requirements, since they are parsed for all
  // extension types.
  CHECK(info);
  return *info;
}

RequirementsHandler::RequirementsHandler() = default;
RequirementsHandler::~RequirementsHandler() = default;

base::span<const char* const> RequirementsHandler::Keys() const {
  static constexpr const char* kKeys[] = {ManifestKeys::kRequirements};
  return kKeys;
}

bool RequirementsHandler::AlwaysParseForType(Manifest::Type type) const {
  return true;
}

bool RequirementsHandler::Parse(Extension* extension, std::u16string* error) {
  ManifestKeys manifest_keys;
  if (!ManifestKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys, *error)) {
    return false;
  }

  auto requirements_info = std::make_unique<RequirementsInfo>();
  if (!manifest_keys.requirements) {
    extension->SetManifestData(ManifestKeys::kRequirements,
                               std::move(requirements_info));
    return true;
  }

  const auto& requirements = *manifest_keys.requirements;

  // The plugins requirement is deprecated. Raise an install warning. If the
  // extension explicitly requires npapi plugins, raise an error.
  if (requirements.plugins) {
    extension->AddInstallWarning(
        InstallWarning(errors::kPluginsRequirementDeprecated));
    if (requirements.plugins->npapi && *requirements.plugins->npapi) {
      *error = errors::kNPAPIPluginsNotSupported;
      return false;
    }
  }

  if (requirements._3d) {
    // css3d is always available, so no check is needed, but no error is
    // generated.
    requirements_info->webgl = base::Contains(
        requirements._3d->features, api::requirements::_3DFeature::kWebgl);
  }

  extension->SetManifestData(ManifestKeys::kRequirements,
                             std::move(requirements_info));
  return true;
}

}  // namespace extensions
