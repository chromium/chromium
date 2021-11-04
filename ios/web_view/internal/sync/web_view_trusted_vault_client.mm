// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/sync/web_view_trusted_vault_client.h"

#include "base/callback.h"
#include "base/notreached.h"
#include "components/signin/public/identity_manager/account_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewTrustedVaultClient::WebViewTrustedVaultClient() {}

WebViewTrustedVaultClient::~WebViewTrustedVaultClient() = default;

void WebViewTrustedVaultClient::AddObserver(Observer* observer) {
  // TODO(crbug.com/1266130): Implement this.
}

void WebViewTrustedVaultClient::RemoveObserver(Observer* observer) {
  // TODO(crbug.com/1266130): Implement this.
}

void WebViewTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
        callback) {
  // TODO(crbug.com/1266130): Implement this.
}

void WebViewTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Not used on iOS.
  NOTREACHED();
}

void WebViewTrustedVaultClient::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/1266130): Implement this.
}

void WebViewTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/1266130): Implement this.
}

void WebViewTrustedVaultClient::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure callback) {
  // Not used on iOS.
  NOTREACHED();
}

}  // namespace ios_web_view
