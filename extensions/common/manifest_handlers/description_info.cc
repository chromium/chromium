// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/description_info.h"

#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

DescriptionInfo::DescriptionInfo(const std::string& description)
    : description_(description) {}

DescriptionInfo::~DescriptionInfo() = default;

// Return the `extension` description.
// static
const std::string& DescriptionInfo::GetDescription(const Extension& extension) {
  const DescriptionInfo* info = static_cast<const DescriptionInfo*>(
      extension.GetManifestData(manifest_keys::kDescription));
  return info ? info->description_ : base::EmptyString();
}

DescriptionHandler::DescriptionHandler() = default;
DescriptionHandler::~DescriptionHandler() = default;

bool DescriptionHandler::Parse(Extension* extension, std::u16string* error) {
  const base::Value* desc_value =
      extension->manifest()->FindKey(manifest_keys::kDescription);
  CHECK(desc_value);
  if (!desc_value->is_string()) {
    *error = manifest_errors::kInvalidDescription;
    return false;
  }

  const std::string& description = desc_value->GetString();
  // If description is empty, we do not need to save it at all.
  if (!description.empty()) {
    extension->SetManifestData(manifest_keys::kDescription,
                               std::make_unique<DescriptionInfo>(description));
  }

  return true;
}

base::span<const char* const> DescriptionHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kDescription};
  return kKeys;
}

}  // namespace extensions
