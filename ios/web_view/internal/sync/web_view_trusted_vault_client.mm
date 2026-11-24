// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_trusted_vault_client.h"

#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/to_vector.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/logging.h"
#import "base/notimplemented.h"
#import "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/trusted_vault/trusted_vault_key_and_version.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"
#import "ios/web_view/internal/sync/cwv_trusted_vault_observer_internal.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_trusted_vault_key.h"
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

std::vector<trusted_vault::TrustedVaultKeyAndVersion> ConvertKeysWithVersions(
    NSArray<CWVTrustedVaultKey*>* shared_keys,
    NSError* error) {
  if (error) {
    DLOG(ERROR) << "Failed to fetch trusted vault keys: "
                << base::SysNSStringToUTF8(error.localizedDescription);
    return {};
  }
  std::vector<trusted_vault::TrustedVaultKeyAndVersion> result;
  result.reserve(shared_keys.count);
  for (CWVTrustedVaultKey* key in shared_keys) {
    result.emplace_back(
        base::ToVector(base::apple::NSDataToSpan(key.keyMaterial)),
        base::checked_cast<int>(key.version));
  }
  return result;
}

std::vector<trusted_vault::TrustedVaultKeyAndVersion> ConvertLegacyKeys(
    NSArray<NSData*>* shared_keys,
    NSError* error) {
  if (error) {
    DLOG(ERROR) << "Failed to fetch trusted vault keys: "
                << base::SysNSStringToUTF8(error.localizedDescription);
    return {};
  }
  std::vector<trusted_vault::TrustedVaultKeyAndVersion> result;
  result.reserve(shared_keys.count);
  for (NSData* data in shared_keys) {
    // TODO(crbug.com/588274186): Remove `ConvertLegacyKeys()` once downstream
    // `CWVTrustedVaultProvider` implementations migrate to
    // `-fetchKeysWithVersionsForIdentity:completion:`. Using version 0 as a
    // temporary placeholder is safe because `ios/web_view` only supports
    // `kChromeSync`, where Sync Nigori decryption ignores key versions.
    result.emplace_back(base::ToVector(base::apple::NSDataToSpan(data)),
                        /*version=*/0);
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

  // TODO(crbug.com/588274186): Remove this adapter once
  // `TrustedVaultClient::FetchKeys()` returns `TrustedVaultKeyAndVersion`.
  base::OnceCallback<void(
      const std::vector<trusted_vault::TrustedVaultKeyAndVersion>&)>
      adapted_callback =
          base::BindOnce([](const std::vector<
                             trusted_vault::TrustedVaultKeyAndVersion>& keys) {
            return base::ToVector(
                keys, &trusted_vault::TrustedVaultKeyAndVersion::key);
          }).Then(std::move(callback));
  CWVIdentity* identity = CWVIdentityFromCoreAccountInfo(account_info);
  // TODO(crbug.com/588274186): Both fetch selectors on
  // `CWVTrustedVaultProvider` are temporarily `@optional` so downstream
  // Swift/ObjC embedders can adopt
  // `-fetchKeysWithVersionsForIdentity:completion:` and delete
  // `-fetchKeysForIdentity:completion:` in one step without failing protocol
  // conformance. Once downstream embedders migrate, make
  // `-fetchKeysWithVersionsForIdentity:completion:` `@required`, remove
  // `-fetchKeysForIdentity:completion:`, and drop both `respondsToSelector:`
  // checks and the fallback below.
  if ([provider respondsToSelector:
                    @selector(fetchKeysWithVersionsForIdentity:completion:)]) {
    [provider fetchKeysWithVersionsForIdentity:identity
                                    completion:base::CallbackToBlock(
                                                   base::BindOnce(
                                                       &ConvertKeysWithVersions)
                                                       .Then(std::move(
                                                           adapted_callback)))];
    return;
  }
  if ([provider
          respondsToSelector:@selector(fetchKeysForIdentity:completion:)]) {
    [provider fetchKeysForIdentity:identity
                        completion:base::CallbackToBlock(
                                       base::BindOnce(&ConvertLegacyKeys)
                                           .Then(std::move(adapted_callback)))];
    return;
  }
  DLOG(ERROR) << "CWVTrustedVaultProvider does not implement key fetching.";
  std::move(adapted_callback).Run({});
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
    base::OnceCallback<void(bool)> callback) {
  // Not used on iOS.
  NOTREACHED();
}

void WebViewTrustedVaultClient::CanEnroll(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/579292060): Implement for iOS.
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

void WebViewTrustedVaultClient::Enroll(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<uint8_t>&, int)> callback) {
  // TODO(crbug.com/579292060): Implement for iOS.
  NOTIMPLEMENTED();
  std::move(callback).Run(std::vector<uint8_t>(), -1);
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
