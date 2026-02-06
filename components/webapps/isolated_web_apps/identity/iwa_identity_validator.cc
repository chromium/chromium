// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/identity/iwa_identity_validator.h"

#include <variant>

#include "base/base64.h"
#include "base/containers/map_util.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/webapps/isolated_web_apps/client.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

namespace web_app {

namespace {

using KeyRotationInfo = IwaRuntimeDataProvider::KeyRotationInfo;

bool Matches(const web_package::PublicKey& public_key,
             const KeyRotationInfo& kr_info) {
  return std::visit(
      [&](const auto& public_key) {
        return std::ranges::equal(public_key.bytes(), kr_info.public_key) ||
               (kr_info.previous_key &&
                std::ranges::equal(public_key.bytes(), *kr_info.previous_key));
      },
      public_key);
}

base::expected<void, std::string>
ValidateWebBundleIdentityAgainstKeyRotationInfo(
    const std::string& web_bundle_id,
    const std::vector<web_package::PublicKey>& public_keys,
    const KeyRotationInfo& kr_info) {
  if (std::ranges::any_of(public_keys, [&](const auto& public_key) {
        return Matches(public_key, kr_info);
      })) {
    return base::ok();
  }

  return base::unexpected(
      base::StringPrintf("Rotated key for Web Bundle ID <%s> doesn't match "
                         "any public key in the signature list.",
                         web_bundle_id.c_str()));
}

}  // namespace

void IwaIdentityValidator::CreateSingleton() {
  static base::NoDestructor<IwaIdentityValidator> instance;
  instance.get();
}

base::expected<void, std::string>
IwaIdentityValidator::ValidateWebBundleIdentity(
    const std::string& web_bundle_id,
    const std::vector<web_package::PublicKey>& public_keys) const {
  if (const auto* provider =
          IwaClient::GetInstance()->GetRuntimeDataProvider()) {
    if (const auto* kr_info = provider->GetKeyRotationInfo(web_bundle_id)) {
      return ValidateWebBundleIdentityAgainstKeyRotationInfo(
          web_bundle_id, public_keys, *kr_info);
    }
  }

  return IdentityValidator::ValidateWebBundleIdentity(web_bundle_id,
                                                      public_keys);
}

}  // namespace web_app
