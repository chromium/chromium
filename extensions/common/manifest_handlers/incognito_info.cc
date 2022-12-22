// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/incognito_info.h"

#include <memory>

#include "extensions/common/extension.h"

namespace extensions {

using IncognitoManifestKeys = api::incognito::ManifestKeys;

IncognitoInfo::IncognitoInfo(api::incognito::IncognitoMode mode) : mode(mode) {
  DCHECK_NE(api::incognito::INCOGNITO_MODE_NONE, mode);
}

IncognitoInfo::~IncognitoInfo() = default;

// static
bool IncognitoInfo::IsSplitMode(const Extension* extension) {
  IncognitoInfo* info = static_cast<IncognitoInfo*>(
      extension->GetManifestData(IncognitoManifestKeys::kIncognito));
  return info->mode == api::incognito::INCOGNITO_MODE_SPLIT;
}

// static
bool IncognitoInfo::IsIncognitoAllowed(const Extension* extension) {
  IncognitoInfo* info = static_cast<IncognitoInfo*>(
      extension->GetManifestData(IncognitoManifestKeys::kIncognito));
  return info->mode != api::incognito::INCOGNITO_MODE_NOT_ALLOWED;
}

IncognitoHandler::IncognitoHandler() = default;
IncognitoHandler::~IncognitoHandler() = default;

bool IncognitoHandler::Parse(Extension* extension, std::u16string* error) {
  IncognitoManifestKeys manifest_keys;
  if (!IncognitoManifestKeys::ParseFromDictionary(
          extension->manifest()->available_values(), &manifest_keys, error)) {
    return false;
  }

  api::incognito::IncognitoMode mode = manifest_keys.incognito;

  // This will be the case if the manifest key was omitted.
  if (mode == api::incognito::INCOGNITO_MODE_NONE) {
    // Extensions and Chrome apps default to spanning mode. Hosted and legacy
    // packaged apps default to split mode.
    mode = extension->is_hosted_app() || extension->is_legacy_packaged_app()
               ? api::incognito::INCOGNITO_MODE_SPLIT
               : api::incognito::INCOGNITO_MODE_SPANNING;
  }

  extension->SetManifestData(IncognitoManifestKeys::kIncognito,
                             std::make_unique<IncognitoInfo>(mode));
  return true;
}

bool IncognitoHandler::AlwaysParseForType(Manifest::Type type) const {
  return true;
}

base::span<const char* const> IncognitoHandler::Keys() const {
  static constexpr const char* kKeys[] = {IncognitoManifestKeys::kIncognito};
  return kKeys;
}

}  // namespace extensions
