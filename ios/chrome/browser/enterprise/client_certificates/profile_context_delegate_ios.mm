// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/profile_context_delegate_ios.h"

#import "base/check.h"
#import "base/memory/scoped_refptr.h"
#import "components/enterprise/client_certificates/core/constants.h"
#import "components/enterprise/client_certificates/core/prefs.h"
#import "net/cert/x509_certificate.h"

namespace client_certificates {

ProfileContextDelegateIOS::ProfileContextDelegateIOS() = default;

ProfileContextDelegateIOS::~ProfileContextDelegateIOS() = default;

void ProfileContextDelegateIOS::OnClientCertificateDeleted(
    scoped_refptr<net::X509Certificate> certificate) {
  // TODO(crbug.com/483299588): Flush stored certificates.
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
