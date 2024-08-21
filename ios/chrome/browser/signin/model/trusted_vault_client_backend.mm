// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"

BASE_FEATURE(kTrustedVaultSecurityDomainKillSwitch,
             "TrustedVaultSecurityDomainKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

TrustedVaultClientBackend::TrustedVaultClientBackend() = default;

TrustedVaultClientBackend::~TrustedVaultClientBackend() = default;
