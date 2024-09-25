// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_TEST_ENVIRONMENT_BROWSER_STATE_ADAPTOR_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_TEST_ENVIRONMENT_BROWSER_STATE_ADAPTOR_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

namespace web {
class BrowserState;
}  // namespace web

// Adaptor for the signin::IdentityTestEnvironment that adds support for
// injecting test services that have been keyed by profile to identity
// management services.
class IdentityTestEnvironmentBrowserStateAdaptor {
 public:
  // Provides an identity manager with fake authentication services that can be
  // used in testing. This manager will be attached to the given browser
  // context.
  static std::unique_ptr<KeyedService> BuildIdentityManagerForTests(
      web::BrowserState* context);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_TEST_ENVIRONMENT_BROWSER_STATE_ADAPTOR_H_
