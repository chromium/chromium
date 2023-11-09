// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_TRUSTED_VAULT_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_TRUSTED_VAULT_API_H_

#include <memory>

#include "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"
#include "ios/chrome/browser/signin/model/trusted_vault_configuration.h"

namespace ios {
namespace provider {

// Creates a new instance of TrustedVaultClientBackend.
std::unique_ptr<TrustedVaultClientBackend> CreateTrustedVaultClientBackend(
    TrustedVaultConfiguration* configuration);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_TRUSTED_VAULT_API_H_
