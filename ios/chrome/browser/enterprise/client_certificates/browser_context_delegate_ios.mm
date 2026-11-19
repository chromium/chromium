// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/browser_context_delegate_ios.h"

#import <optional>
#import <string>

#import "base/check_deref.h"
#import "base/memory/scoped_refptr.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/client_certificates/core/constants.h"
#import "components/enterprise/client_certificates/core/prefs.h"
#import "components/prefs/pref_service.h"
#import "net/cert/x509_certificate.h"

namespace client_certificates {

BrowserContextDelegateIOS::BrowserContextDelegateIOS(PrefService* local_state)
    : local_state_(CHECK_DEREF(local_state)) {}

BrowserContextDelegateIOS::~BrowserContextDelegateIOS() = default;

void BrowserContextDelegateIOS::OnClientCertificateDeleted(
    scoped_refptr<net::X509Certificate> certificate) {
  // No cleanup required on iOS. Certificates will be cleaned up upon profile
  // directory deletion.
}

std::string BrowserContextDelegateIOS::GetExpectedCommonName() const {
  if (std::optional<std::string> computer_name =
          GetConfiguredComputerName(&local_state_.get())) {
    return *computer_name;
  }
  return policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
}

std::string BrowserContextDelegateIOS::GetIdentityName() {
  return kManagedBrowserIdentityName;
}

std::string BrowserContextDelegateIOS::GetTemporaryIdentityName() {
  return kTemporaryManagedBrowserIdentityName;
}

std::string BrowserContextDelegateIOS::GetPolicyPref() {
  return prefs::kProvisionManagedClientCertificateForBrowserPrefs;
}

std::string BrowserContextDelegateIOS::GetConfigPolicyPref() const {
  return prefs::kManagedClientCertificateForBrowserConfigPrefs;
}

std::string BrowserContextDelegateIOS::GetLoggingContext() {
  return "Browser";
}

}  // namespace client_certificates
