// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/trusted_vault_api.h"

#import "base/functional/callback.h"
#import "base/notreached.h"

namespace ios {
namespace provider {
namespace {

// A null implementation of TrustedVaultClient used for the public builds. It
// fails all method calls (as it should only be called after the user has been
// signed-in which is not supported by public build).
class ChromiumTrustedVaultClientBackend final
    : public TrustedVaultClientBackend {
 public:
  // TrustedVaultClientBackend implementation.
  void AddObserver(Observer* observer) final;
  void RemoveObserver(Observer* observer) final;
  void SetDeviceRegistrationPublicKeyVerifierForUMA(
      VerifierCallback verifier) final;
  void FetchKeys(id<SystemIdentity> identity,
                 NSString* security_domain,
                 KeyFetchedCallback completion) final;
  void MarkLocalKeysAsStale(id<SystemIdentity> identity,
                            NSString* security_domain,
                            base::OnceClosure completion) final;
  void GetDegradedRecoverabilityStatus(
      id<SystemIdentity> identity,
      NSString* security_domain,
      base::OnceCallback<void(bool)> completion) final;
  void Reauthentication(id<SystemIdentity> identity,
                        NSString* security_domain,
                        UIViewController* presenting_view_controller,
                        CompletionBlock completion) final;
  void FixDegradedRecoverability(id<SystemIdentity> identity,
                                 NSString* security_domain,
                                 UIViewController* presenting_view_controller,
                                 CompletionBlock completion) final;
  void CancelDialog(BOOL animated, ProceduralBlock completion) final;
  void ClearLocalData(id<SystemIdentity> identity,
                      NSString* security_domain,
                      base::OnceCallback<void(bool)> completion) final;
  void GetPublicKeyForIdentity(id<SystemIdentity> identity,
                               GetPublicKeyCallback completion) final;
};

void ChromiumTrustedVaultClientBackend::AddObserver(Observer* observer) {
  // Do nothing.
}

void ChromiumTrustedVaultClientBackend::RemoveObserver(Observer* observer) {
  // Do nothing.
}

void ChromiumTrustedVaultClientBackend::
    SetDeviceRegistrationPublicKeyVerifierForUMA(VerifierCallback verifier) {
  // Do nothing.
}

void ChromiumTrustedVaultClientBackend::FetchKeys(
    id<SystemIdentity> identity,
    NSString* security_domain,
    KeyFetchedCallback completion) {
  NOTREACHED_IN_MIGRATION();
}

void ChromiumTrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    NSString* security_domain,
    base::OnceClosure completion) {
  NOTREACHED_IN_MIGRATION();
}

void ChromiumTrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    NSString* security_domain,
    base::OnceCallback<void(bool)> completion) {
  NOTREACHED_IN_MIGRATION();
}

void ChromiumTrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    NSString* security_domain,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED_IN_MIGRATION();
}

void ChromiumTrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    NSString* security_domain,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED_IN_MIGRATION();
}

void ChromiumTrustedVaultClientBackend::CancelDialog(BOOL animated,
                                                     ProceduralBlock callback) {
  NOTREACHED_IN_MIGRATION();
}

void ChromiumTrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    NSString* security_domain,
    base::OnceCallback<void(bool)> completion) {
  // Do nothing.
}

void ChromiumTrustedVaultClientBackend::GetPublicKeyForIdentity(
    id<SystemIdentity> identity,
    GetPublicKeyCallback completion) {
  NOTREACHED_IN_MIGRATION();
}

}  // anonymous namespace

std::unique_ptr<TrustedVaultClientBackend> CreateTrustedVaultClientBackend(
    TrustedVaultConfiguration* configuration) {
  return std::make_unique<ChromiumTrustedVaultClientBackend>();
}

}  // namespace provider
}  // namespace ios
