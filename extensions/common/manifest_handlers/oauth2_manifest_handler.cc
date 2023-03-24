// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/api/oauth2.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

using OAuth2ManifestKeys = api::oauth2::ManifestKeys;
using OAuth2Info = api::oauth2::OAuth2Info;

namespace errors = manifest_errors;

// A wrapper for `OAuth2Info` which inherits from `ManifestData`.
struct OAuth2ManifestData : Extension::ManifestData {
  OAuth2Info info;
};

}  // namespace

OAuth2ManifestHandler::OAuth2ManifestHandler() = default;
OAuth2ManifestHandler::~OAuth2ManifestHandler() = default;

// static
const OAuth2Info& OAuth2ManifestHandler::GetOAuth2Info(
    const Extension& extension) {
  static const base::NoDestructor<OAuth2Info> empty_oauth2_info;
  OAuth2ManifestData* data = static_cast<OAuth2ManifestData*>(
      extension.GetManifestData(OAuth2ManifestKeys::kOauth2));
  return data ? data->info : *empty_oauth2_info;
}

bool OAuth2ManifestHandler::Parse(Extension* extension, std::u16string* error) {
  OAuth2ManifestKeys manifest_keys;
  if (!OAuth2ManifestKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys, *error)) {
    return false;
  }

  OAuth2Info& info = manifest_keys.oauth2;

  // Allowlisted component apps (where the allowlisting is enforced by the
  // features files) using `auto_approve` may use Chrome's client ID by omitting
  // the field.
  bool can_omit_client_id =
      extension->location() == mojom::ManifestLocation::kComponent &&
      info.auto_approve && *info.auto_approve;

  if ((!info.client_id || info.client_id->empty()) && !can_omit_client_id) {
    *error = errors::kInvalidOAuth2ClientId;
    return false;
  }

  auto manifest_data = std::make_unique<OAuth2ManifestData>();
  manifest_data->info = std::move(info);
  extension->SetManifestData(OAuth2ManifestKeys::kOauth2,
                             std::move(manifest_data));
  return true;
}

base::span<const char* const> OAuth2ManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {OAuth2ManifestKeys::kOauth2};
  return kKeys;
}

}  // namespace extensions
