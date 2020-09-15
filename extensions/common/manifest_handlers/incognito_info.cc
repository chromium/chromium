// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
  return info ? info->mode == api::incognito::INCOGNITO_MODE_SPLIT : false;
}

// static
bool IncognitoInfo::IsIncognitoAllowed(const Extension* extension) {
  IncognitoInfo* info = static_cast<IncognitoInfo*>(
      extension->GetManifestData(IncognitoManifestKeys::kIncognito));
  return info ? info->mode != api::incognito::INCOGNITO_MODE_NOT_ALLOWED : true;
}

IncognitoHandler::IncognitoHandler() = default;
IncognitoHandler::~IncognitoHandler() = default;

bool IncognitoHandler::Parse(Extension* extension, base::string16* error) {
  // Extensions and Chrome apps default to spanning mode. Hosted and legacy
  // packaged apps default to split mode.
  api::incognito::IncognitoMode default_mode =
      extension->is_hosted_app() || extension->is_legacy_packaged_app()
          ? api::incognito::INCOGNITO_MODE_SPLIT
          : api::incognito::INCOGNITO_MODE_SPANNING;

  // This check is necessary since the "incognito" manifest key may not be
  // available to the extension.
  if (!extension->manifest()->HasKey(IncognitoManifestKeys::kIncognito)) {
    extension->SetManifestData(IncognitoManifestKeys::kIncognito,
                               std::make_unique<IncognitoInfo>(default_mode));
    return true;
  }

  IncognitoManifestKeys manifest_keys;
  if (!IncognitoManifestKeys::ParseFromDictionary(
          *extension->manifest()->value(), &manifest_keys, error)) {
    return false;
  }

  api::incognito::IncognitoMode mode = manifest_keys.incognito;

  // This will be the case if the manifest key was omitted.
  if (mode == api::incognito::INCOGNITO_MODE_NONE)
    mode = default_mode;

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
