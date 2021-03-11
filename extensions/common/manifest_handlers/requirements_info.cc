// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/requirements_info.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

RequirementsInfo::RequirementsInfo(const Manifest* manifest)
    : webgl(false), window_shape(false) {}

RequirementsInfo::~RequirementsInfo() {
}

// static
const RequirementsInfo& RequirementsInfo::GetRequirements(
    const Extension* extension) {
  RequirementsInfo* info = static_cast<RequirementsInfo*>(
      extension->GetManifestData(keys::kRequirements));

  // We should be guaranteed to have requirements, since they are parsed for all
  // extension types.
  CHECK(info);
  return *info;
}

RequirementsHandler::RequirementsHandler() {
}

RequirementsHandler::~RequirementsHandler() {
}

base::span<const char* const> RequirementsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kRequirements};
  return kKeys;
}

bool RequirementsHandler::AlwaysParseForType(Manifest::Type type) const {
  return true;
}

bool RequirementsHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<RequirementsInfo> requirements(
      new RequirementsInfo(extension->manifest()));

  if (!extension->manifest()->HasKey(keys::kRequirements)) {
    extension->SetManifestData(keys::kRequirements, std::move(requirements));
    return true;
  }

  const base::Value* requirements_value = nullptr;
  if (!extension->manifest()->GetDictionary(keys::kRequirements,
                                            &requirements_value)) {
    *error = base::ASCIIToUTF16(errors::kInvalidRequirements);
    return false;
  }

  for (const auto& entry : requirements_value->DictItems()) {
    if (!entry.second.is_dict()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidRequirement,
                                                   entry.first);
      return false;
    }
    const base::Value& requirement_value = entry.second;

    // The plugins requirement is deprecated. Raise an install warning. If the
    // extension explicitly requires npapi plugins, raise an error.
    if (entry.first == "plugins") {
      extension->AddInstallWarning(
          InstallWarning(errors::kPluginsRequirementDeprecated));
      const base::Value* npapi_requirement =
          requirement_value.FindKeyOfType("npapi", base::Value::Type::BOOLEAN);
      if (npapi_requirement != nullptr && npapi_requirement->GetBool()) {
        *error = base::ASCIIToUTF16(errors::kNPAPIPluginsNotSupported);
        return false;
      }
    } else if (entry.first == "3D") {
      const base::Value* features =
          requirement_value.FindKeyOfType("features", base::Value::Type::LIST);
      if (features == nullptr) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidRequirement, entry.first);
        return false;
      }

      for (const auto& feature : features->GetList()) {
        if (!feature.is_string())
          continue;
        if (feature.GetString() == "webgl") {
          requirements->webgl = true;
        } else if (feature.GetString() == "css3d") {
          // css3d is always available, so no check is needed, but no error is
          // generated.
        } else {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidRequirement, entry.first);
          return false;
        }
      }
    } else if (entry.first == "window") {
      for (const auto& feature : requirement_value.DictItems()) {
        if (!feature.second.is_bool()) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidRequirement, entry.first);
          return false;
        }
        if (feature.first == "shape") {
          requirements->window_shape = feature.second.GetBool();
        } else {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidRequirement, entry.first);
          return false;
        }
      }
    } else {
      *error = base::ASCIIToUTF16(errors::kInvalidRequirements);
      return false;
    }
  }

  extension->SetManifestData(keys::kRequirements, std::move(requirements));
  return true;
}

}  // namespace extensions
