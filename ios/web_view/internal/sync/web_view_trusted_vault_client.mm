// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_trusted_vault_client.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/logging.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"
#import "ios/web_view/internal/sync/cwv_trusted_vault_observer_internal.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_trusted_vault_provider.h"

namespace ios_web_view {

namespace {

// Converts `account_info` to a CWVIdentity instance.
CWVIdentity* CWVIdentityFromCoreAccountInfo(
    const CoreAccountInfo& account_info) {
  return [[CWVIdentity alloc]
      initWithEmail:base::SysUTF8ToNSString(account_info.email)
           fullName:nil
             gaiaID:account_info.gaia.ToNSString()];
}

// Converts `shared_keys` to std::vector<std::vector<uint8_t>>.
std::vector<std::vector<uint8_t>> ConvertKeys(NSArray<NSData*>* shared_keys,
                                              NSError*) {
  std::vector<std::vector<uint8_t>> result;
  for (NSData* data in shared_keys) {
    auto span = base::apple::NSDataToSpan(data);
    result.emplace_back(span.begin(), span.end());
  }
  return result;
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

  [provider fetchKeysForIdentity:CWVIdentityFromCoreAccountInfo(account_info)
                      completion:base::CallbackToBlock(
                                     base::BindOnce(&ConvertKeys)
                                         .Then(std::move(callback)))];
}

void WebViewTrustedVaultClient::StoreKeys(
    const GaiaId& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version,
    std::optional<trusted_vault::TrustedVaultUserActionTriggerForUMA> trigger) {
  // Not used on iOS.
  NOTREACHED();
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
    const GaiaId& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure callback) {
  // Not used on iOS.
  NOTREACHED();
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
