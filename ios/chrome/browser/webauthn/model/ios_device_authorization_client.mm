// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_device_authorization_client.h"

#import <Foundation/Foundation.h>

#import <string>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/flat_map.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/rand_util.h"
#import "base/strings/string_view_util.h"
#import "components/metrics/metrics_reporting_choice_service.h"
#import "components/prefs/pref_service.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "components/webauthn/core/browser/device_authorization/proto/device_authorization_key.pb.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/webauthn/model/ios_device_authorization_util.h"
#import "ios/chrome/common/credential_provider/device_authorization_key_store.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider.h"
#import "ios/public/provider/chrome/browser/device_attestation/device_integrity_service.h"

namespace {

// Size of the random salt in bytes.
constexpr size_t kSaltLength = 16;

// Invoked when the device integrity snapshot is fetched.
void OnDeviceIntegritySnapshotFetched(
    sync_pb::GetDeviceAuthorizationKeyRequest request,
    std::string salt,
    webauthn::PopulatePlatformDataCallback callback,
    NSData* snapshot) {
  if (snapshot && snapshot.length > 0) {
    sync_pb::GetDeviceAuthorizationKeyRequest::IosGuardSignals* signals =
        request.mutable_ios_guard_signals();
    signals->set_signals(
        base::as_string_view(base::apple::NSDataToSpan(snapshot)));
    signals->set_salt(std::move(salt));
  }
  std::move(callback).Run(std::move(request));
}

}  // namespace

IOSDeviceAuthorizationClient::IOSDeviceAuthorizationClient()
    : passkey_keychain_provider_(std::make_unique<PasskeyKeychainProvider>(
          metrics::MetricsReportingChoiceService::
              IsBasicMetricsReportingEnabled(
                  GetApplicationContext()->GetLocalState()))) {}

IOSDeviceAuthorizationClient::~IOSDeviceAuthorizationClient() = default;

std::optional<webauthn::DeviceAuthorizationKeys>
IOSDeviceAuthorizationClient::GetCachedKeys(const GaiaId& gaia_id) {
  return GetDeviceAuthorizationKeys(gaia_id.ToString());
}

bool IOSDeviceAuthorizationClient::StoreKeys(
    const GaiaId& gaia_id,
    const webauthn::DeviceAuthorizationKeys& keys) {
  return StoreDeviceAuthorizationKeys(gaia_id.ToString(), keys);
}

void IOSDeviceAuthorizationClient::PopulatePlatformData(
    const GaiaId& gaia_id,
    sync_pb::GetDeviceAuthorizationKeyRequest request,
    webauthn::PopulatePlatformDataCallback callback) {
  passkey_keychain_provider_->FetchKeys(
      gaia_id.ToNSString(), webauthn::ReauthenticatePurpose::kDecrypt,
      base::BindOnce(&IOSDeviceAuthorizationClient::OnPasskeyKeysFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback)));
}

void IOSDeviceAuthorizationClient::OnPasskeyKeysFetched(
    sync_pb::GetDeviceAuthorizationKeyRequest request,
    webauthn::PopulatePlatformDataCallback callback,
    webauthn::SharedKeyList keys,
    NSError* error) {
  webauthn::TrustedVaultKeyAvailability* availability =
      request.add_trusted_vault_key_availability();
  availability->set_security_domain(trusted_vault::kPasskeysSecurityDomainName);
  availability->set_key_available(!keys.empty());

  FetchDeviceIntegritySignals(std::move(request), std::move(callback));
}

void IOSDeviceAuthorizationClient::FetchDeviceIntegritySignals(
    sync_pb::GetDeviceAuthorizationKeyRequest request,
    webauthn::PopulatePlatformDataCallback callback) {
  std::string salt = base::RandBytesAsString(kSaltLength);
  GetApplicationContext()->GetDeviceIntegrityService()->FetchSnapshot(
      {.purpose = DeviceIntegrityPurpose::kPasskeys,
       .content_bindings = BuildDeviceIntegrityContentBindings(request, salt)},
      base::BindOnce(&OnDeviceIntegritySnapshotFetched, std::move(request),
                     std::move(salt), std::move(callback)));
}
