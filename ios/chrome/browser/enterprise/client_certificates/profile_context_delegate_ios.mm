// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/profile_context_delegate_ios.h"

#import "base/check.h"
#import "base/memory/scoped_refptr.h"
#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "components/enterprise/client_certificates/core/constants.h"
#import "components/enterprise/client_certificates/core/prefs.h"
#import "net/cert/x509_certificate.h"

namespace client_certificates {

ProfileContextDelegateIOS::ProfileContextDelegateIOS(
    enterprise::ProfileIdService* profile_id_service)
    : profile_id_service_(profile_id_service) {
  CHECK(profile_id_service_);
}

ProfileContextDelegateIOS::~ProfileContextDelegateIOS() = default;

void ProfileContextDelegateIOS::OnClientCertificateDeleted(
    scoped_refptr<net::X509Certificate> certificate) {
  // No cleanup required on iOS. Certificates will be cleaned up upon profile
  // directory deletion.
}

std::string ProfileContextDelegateIOS::GetExpectedCommonName() const {
  return profile_id_service_->GetProfileId().value_or(std::string());
}

std::string ProfileContextDelegateIOS::GetIdentityName() {
  return kManagedProfileIdentityName;
}

std::string ProfileContextDelegateIOS::GetTemporaryIdentityName() {
  return kTemporaryManagedProfileIdentityName;
}

std::string ProfileContextDelegateIOS::GetPolicyPref() {
  return prefs::kProvisionManagedClientCertificateForUserPrefs;
}

std::string ProfileContextDelegateIOS::GetLoggingContext() {
  return "Profile";
}

}  // namespace client_certificates
