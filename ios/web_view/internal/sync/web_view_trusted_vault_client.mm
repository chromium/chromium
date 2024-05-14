// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/sync/web_view_trusted_vault_client.h"

#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/signin/public/identity_manager/account_info.h"
#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"
#import "ios/web_view/internal/sync/cwv_trusted_vault_observer_internal.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_trusted_vault_provider.h"

namespace ios_web_view {

namespace {
CWVIdentity* CWVIdentityFromCoreAccountInfo(
    const CoreAccountInfo& account_info) {
  return [[CWVIdentity alloc]
      initWithEmail:base::SysUTF8ToNSString(account_info.email)
           fullName:nil
             gaiaID:base::SysUTF8ToNSString(account_info.gaia)];
}
}  // namespace

WebViewTrustedVaultClient::WebViewTrustedVaultClient() {}

WebViewTrustedVaultClient::~WebViewTrustedVaultClient() = default;

void WebViewTrustedVaultClient::AddObserver(Observer* observer) {
  id<CWVTrustedVaultProvider> provider = CWVSyncController.trustedVaultProvider;
  if (!provider) {
    DLOG(ERROR) << "Please set CWVSyncController.trustedVaultProvider to "
                   "enable trusted vault.";
    return;
  }

  CWVTrustedVaultObserver* wrappedObserver =
      [[CWVTrustedVaultObserver alloc] initWithTrustedVaultObserver:observer];
  [provider addTrustedVaultObserver:wrappedObserver];
}

void WebViewTrustedVaultClient::RemoveObserver(Observer* observer) {
  id<CWVTrustedVaultProvider> provider = CWVSyncController.trustedVaultProvider;
  if (!provider) {
    DLOG(ERROR) << "Please set CWVSyncController.trustedVaultProvider to "
                   "enable trusted vault.";
    return;
  }

  CWVTrustedVaultObserver* wrappedObserver =
      [[CWVTrustedVaultObserver alloc] initWithTrustedVaultObserver:observer];
  [provider removeTrustedVaultObserver:wrappedObserver];
}

void WebViewTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
        callback) {
  id<CWVTrustedVaultProvider> provider = CWVSyncController.trustedVaultProvider;
  if (!provider) {
    DLOG(ERROR) << "Please set CWVSyncController.trustedVaultProvider to "
                   "enable trusted vault.";
    return;
  }

  __block auto blockCallback = std::move(callback);
  [provider
      fetchKeysForIdentity:CWVIdentityFromCoreAccountInfo(account_info)
                completion:^(NSArray<NSData*>* shared_keys, NSError* error) {
                  // TODO(crbug.com/40204010): Share this logic with
                  // //ios/chrome.
                  std::vector<std::vector<uint8_t>> shared_key_vector;
                  for (NSData* data in shared_keys) {
                    const uint8_t* buffer =
                        static_cast<const uint8_t*>(data.bytes);
                    std::vector<uint8_t> value(buffer, buffer + data.length);
                    shared_key_vector.push_back(value);
                  }
                  std::move(blockCallback).Run(shared_key_vector);
                }];
}

void WebViewTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Not used on iOS.
  NOTREACHED_IN_MIGRATION();
}

void WebViewTrustedVaultClient::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  id<CWVTrustedVaultProvider> provider = CWVSyncController.trustedVaultProvider;
  if (!provider) {
    DLOG(ERROR) << "Please set CWVSyncController.trustedVaultProvider to "
                   "enable trusted vault.";
    return;
  }

  __block auto blockCallback = std::move(callback);
  [provider markLocalKeysAsStaleForIdentity:CWVIdentityFromCoreAccountInfo(
                                                account_info)
                                 completion:^(NSError* error) {
                                   std::move(blockCallback).Run(!error);
                                 }];
}

void WebViewTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  id<CWVTrustedVaultProvider> provider = CWVSyncController.trustedVaultProvider;
  if (!provider) {
    DLOG(ERROR) << "Please set CWVSyncController.trustedVaultProvider to "
                   "enable trusted vault.";
    return;
  }

  __block auto blockCallback = std::move(callback);
  [provider
      isRecoverabilityDegradedForIdentity:CWVIdentityFromCoreAccountInfo(
                                              account_info)
                               completion:^(BOOL degraded, NSError* error) {
                                 std::move(blockCallback).Run(degraded);
                               }];
}

void WebViewTrustedVaultClient::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure callback) {
  // Not used on iOS.
  NOTREACHED_IN_MIGRATION();
}

void WebViewTrustedVaultClient::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  id<CWVTrustedVaultProvider> provider = CWVSyncController.trustedVaultProvider;
  if (!provider) {
    DLOG(ERROR) << "Please set CWVSyncController.trustedVaultProvider to "
                   "enable trusted vault.";
    return;
  }

  [provider clearLocalDataForForIdentity:CWVIdentityFromCoreAccountInfo(
                                             account_info)];
}

}  // namespace ios_web_view
