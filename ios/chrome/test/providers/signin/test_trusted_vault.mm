// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/trusted_vault_api.h"

#import "ios/chrome/test/providers/signin/fake_trusted_vault_client_backend.h"

namespace ios {
namespace provider {

std::unique_ptr<TrustedVaultClientBackend> CreateTrustedVaultClientBackend(
    TrustedVaultConfiguration* configuration) {
  return std::make_unique<FakeTrustedVaultClientBackend>();
}

}  // namespace provider
}  // namespace ios
