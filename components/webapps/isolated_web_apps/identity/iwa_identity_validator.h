// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IDENTITY_IWA_IDENTITY_VALIDATOR_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IDENTITY_IWA_IDENTITY_VALIDATOR_H_

#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/identity_validator.h"

namespace web_app {

class IwaIdentityValidator : public web_package::IdentityValidator {
 public:
  // Creates a global singleton that can be accessed via
  // `web_package::IdentityValidator::GetInstance()`.
  static void CreateSingleton();

  // web_package::IdentityValidator:
  base::expected<void, std::string> ValidateWebBundleIdentity(
      const std::string& web_bundle_id,
      const std::vector<web_package::PublicKey>& public_keys) const override;

  // Same as above, but allows disabling "soft" key rotation (i.e. accepting
  // "previous" keys that are still trusted for execution, but shouldn't be used
  // for fresh installs or updates).
  // See go/iwa-soft-key-rotation for more details.
  static base::expected<void, std::string> ValidateWebBundleIdentity(
      const std::string& web_bundle_id,
      const std::vector<web_package::PublicKey>& public_keys,
      bool allow_soft_key_rotation);

 private:
  IwaIdentityValidator() = default;
  ~IwaIdentityValidator() override = default;

  friend base::NoDestructor<IwaIdentityValidator>;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_IDENTITY_IWA_IDENTITY_VALIDATOR_H_
