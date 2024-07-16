// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"

#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"

namespace {

std::string GetDefaultSecurityDomainPath() {
  return GetSecurityDomainPath(trusted_vault::SecurityDomainId::kChromeSync);
}

}  // namespace

TrustedVaultClientBackend::TrustedVaultClientBackend() = default;

TrustedVaultClientBackend::~TrustedVaultClientBackend() = default;

void TrustedVaultClientBackend::AddObserver(
    TrustedVaultClientBackend::Observer* observer,
    const std::string& security_domain_path) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::RemoveObserver(
    Observer* observer,
    const std::string& security_domain_path) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::AddObserver(Observer* observer) {
  AddObserver(observer, GetDefaultSecurityDomainPath());
}

void TrustedVaultClientBackend::RemoveObserver(Observer* observer) {
  RemoveObserver(observer, GetDefaultSecurityDomainPath());
}

void TrustedVaultClientBackend::FetchKeys(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    KeyFetchedCallback completion) {
  FetchKeys(identity, base::SysUTF8ToNSString(security_domain_path),
            std::move(completion));
}

void TrustedVaultClientBackend::FetchKeys(id<SystemIdentity> identity,
                                          NSString* security_domain_path,
                                          KeyFetchedCallback completion) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    base::OnceClosure completion) {
  MarkLocalKeysAsStale(identity, base::SysUTF8ToNSString(security_domain_path),
                       std::move(completion));
}

void TrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    NSString* security_domain_path,
    base::OnceClosure completion) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    base::OnceCallback<void(bool)> completion) {
  GetDegradedRecoverabilityStatus(identity,
                                  base::SysUTF8ToNSString(security_domain_path),
                                  std::move(completion));
}

void TrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    NSString* security_domain_path,
    base::OnceCallback<void(bool)> completion) {
  NOTREACHED_NORETURN();
}

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::ReauthenticationWithCancelCallback(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  Reauthentication(identity, base::SysUTF8ToNSString(security_domain_path),
                   presenting_view_controller, completion);
  base::WeakPtr<TrustedVaultClientBackend> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();
  return base::BindOnce(
      [](base::WeakPtr<TrustedVaultClientBackend> weak_ptr, bool animated,
         ProceduralBlock cancel_done_callback) {
        weak_ptr->CancelDialog(animated, cancel_done_callback);
      },
      weak_ptr);
}

void TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    NSString* security_domain_path,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED_NORETURN();
}

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::FixDegradedRecoverabilityWithCancelCallback(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  FixDegradedRecoverability(identity,
                            base::SysUTF8ToNSString(security_domain_path),
                            presenting_view_controller, completion);
  base::WeakPtr<TrustedVaultClientBackend> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();
  return base::BindOnce(
      [](base::WeakPtr<TrustedVaultClientBackend> weak_ptr, bool animated,
         ProceduralBlock cancel_done_callback) {
        weak_ptr->CancelDialog(animated, cancel_done_callback);
      },
      weak_ptr);
}

void TrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    NSString* security_domain_path,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::CancelDialog(BOOL animated,
                                             ProceduralBlock callback) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    base::OnceCallback<void(bool)> completion) {
  ClearLocalData(identity, base::SysUTF8ToNSString(security_domain_path),
                 std::move(completion));
}

void TrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    NSString* security_domain_path,
    base::OnceCallback<void(bool)> completion) {
  NOTREACHED_NORETURN();
}
