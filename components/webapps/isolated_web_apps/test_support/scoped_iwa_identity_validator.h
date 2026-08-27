// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_SCOPED_IWA_IDENTITY_VALIDATOR_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_SCOPED_IWA_IDENTITY_VALIDATOR_H_

#include <optional>

#include "base/auto_reset.h"
#include "components/web_package/signed_web_bundles/identity_validator.h"
#include "components/webapps/isolated_web_apps/identity/iwa_identity_validator.h"

namespace web_app::test {

// A test-only IdentityValidator that routes validation to IwaIdentityValidator
// and is scoped to the lifetime of the object, cleanly resetting the global
// IdentityValidator instance on destruction.
class ScopedIwaIdentityValidator : public IwaIdentityValidator {
 public:
  ScopedIwaIdentityValidator();
  ~ScopedIwaIdentityValidator() override;

 private:
  std::optional<base::AutoReset<web_package::IdentityValidator*>> auto_reset_;
};

}  // namespace web_app::test

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_SCOPED_IWA_IDENTITY_VALIDATOR_H_
