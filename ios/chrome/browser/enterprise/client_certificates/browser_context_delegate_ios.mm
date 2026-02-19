// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/browser_context_delegate_ios.h"

#import "base/memory/scoped_refptr.h"
#import "components/enterprise/client_certificates/core/constants.h"
#import "components/enterprise/client_certificates/core/prefs.h"
#import "net/cert/x509_certificate.h"

namespace client_certificates {

BrowserContextDelegateIOS::BrowserContextDelegateIOS() = default;
BrowserContextDelegateIOS::~BrowserContextDelegateIOS() = default;

void BrowserContextDelegateIOS::OnClientCertificateDeleted(
    scoped_refptr<net::X509Certificate> certificate) {
  // TODO(crbug.com/483299588): flush stored certificates
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

std::string BrowserContextDelegateIOS::GetLoggingContext() {
  return "Browser";
}

}  // namespace client_certificates
