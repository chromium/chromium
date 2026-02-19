// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_BROWSER_CONTEXT_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_BROWSER_CONTEXT_DELEGATE_IOS_H_

#import "components/enterprise/client_certificates/core/context_delegate.h"

namespace client_certificates {

class BrowserContextDelegateIOS : public ContextDelegate {
 public:
  BrowserContextDelegateIOS();
  ~BrowserContextDelegateIOS() override;

  // ContextDelegate:
  void OnClientCertificateDeleted(
      scoped_refptr<net::X509Certificate> certificate) override;
  std::string GetIdentityName() override;
  std::string GetTemporaryIdentityName() override;
  std::string GetPolicyPref() override;
  std::string GetLoggingContext() override;
};

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_BROWSER_CONTEXT_DELEGATE_IOS_H_
