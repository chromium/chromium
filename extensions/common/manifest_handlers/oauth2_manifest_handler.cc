// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"

#include <stddef.h>

#include <memory>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

namespace {

// Manifest keys.
const char kClientId[] = "client_id";
const char kScopes[] = "scopes";
const char kAutoApprove[] = "auto_approve";

}  // namespace

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

OAuth2Info::OAuth2Info() : auto_approve(false) {}
OAuth2Info::~OAuth2Info() {}

static base::LazyInstance<OAuth2Info>::DestructorAtExit g_empty_oauth2_info =
    LAZY_INSTANCE_INITIALIZER;

// static
const OAuth2Info& OAuth2Info::GetOAuth2Info(const Extension* extension) {
  OAuth2Info* info = static_cast<OAuth2Info*>(
      extension->GetManifestData(keys::kOAuth2));
  return info ? *info : g_empty_oauth2_info.Get();
}

OAuth2ManifestHandler::OAuth2ManifestHandler() {
}

OAuth2ManifestHandler::~OAuth2ManifestHandler() {
}

bool OAuth2ManifestHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<OAuth2Info> info(new OAuth2Info);
  const base::Value* dict = nullptr;
  if (!extension->manifest()->GetDictionary(keys::kOAuth2, &dict)) {
    *error = base::ASCIIToUTF16(errors::kInvalidOAuth2ClientId);
    return false;
  }

  // HasPath checks for whether the manifest is allowed to have
  // oauth2.auto_approve based on whitelist, and if it is present.
  // GetBoolean reads the value of auto_approve directly from dict to prevent
  // duplicate checking.
  if (extension->manifest()->HasPath(keys::kOAuth2AutoApprove)) {
    const base::Value* auto_approve =
        dict->FindKeyOfType(kAutoApprove, base::Value::Type::BOOLEAN);
    if (auto_approve == nullptr) {
      *error = base::ASCIIToUTF16(errors::kInvalidOAuth2AutoApprove);
      return false;
    }
    info->auto_approve = auto_approve->GetBool();
  }

  // Component apps using auto_approve may use Chrome's client ID by
  // omitting the field.
  const base::Value* client_id =
      dict->FindKeyOfType(kClientId, base::Value::Type::STRING);
  if (client_id != nullptr)
    info->client_id = client_id->GetString();

  if (info->client_id.empty() &&
      (extension->location() != mojom::ManifestLocation::kComponent ||
       !info->auto_approve)) {
    *error = base::ASCIIToUTF16(errors::kInvalidOAuth2ClientId);
    return false;
  }
  const base::Value* list =
      dict->FindKeyOfType(kScopes, base::Value::Type::LIST);
  if (list == nullptr) {
    *error = base::ASCIIToUTF16(errors::kInvalidOAuth2Scopes);
    return false;
  }

  for (const auto& entry : list->GetList()) {
    if (!entry.is_string()) {
      *error = base::ASCIIToUTF16(errors::kInvalidOAuth2Scopes);
      return false;
    }
    info->scopes.push_back(entry.GetString());
  }

  extension->SetManifestData(keys::kOAuth2, std::move(info));
  return true;
}

base::span<const char* const> OAuth2ManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kOAuth2};
  return kKeys;
}

}  // namespace extensions
