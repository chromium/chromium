// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_IDENTITY_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_IDENTITY_API_H_

#include <memory>

#include "ios/chrome/browser/signin/model/system_identity_manager.h"
#include "ios/public/provider/chrome/browser/signin/signin_sso_api.h"

namespace ios {
namespace provider {

// Creates a new SystemIdentityManager instance. Returns null if signin
// is not supported by the application.
std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager(
    id<SingleSignOnService> sso_service);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_SIGNIN_IDENTITY_API_H_
