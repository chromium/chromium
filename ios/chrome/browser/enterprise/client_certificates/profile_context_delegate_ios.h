// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CONTEXT_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CONTEXT_DELEGATE_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/enterprise/client_certificates/core/context_delegate.h"

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace client_certificates {

class ProfileContextDelegateIOS : public ContextDelegate {
 public:
  explicit ProfileContextDelegateIOS(
      enterprise::ProfileIdService* profile_id_service);
  ~ProfileContextDelegateIOS() override;

  // ContextDelegate:
  void OnClientCertificateDeleted(
      scoped_refptr<net::X509Certificate> certificate) override;
  std::string GetExpectedCommonName() const override;
  std::string GetIdentityName() override;
  std::string GetTemporaryIdentityName() override;
  std::string GetPolicyPref() override;
  std::string GetLoggingContext() override;

 private:
  const raw_ptr<enterprise::ProfileIdService> profile_id_service_;
};

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_PROFILE_CONTEXT_DELEGATE_IOS_H_
