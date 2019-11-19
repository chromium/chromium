// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TOKEN_VALIDATOR_BASE_H_
#define REMOTING_HOST_TOKEN_VALIDATOR_BASE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/ssl/client_cert_identity.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/third_party_auth_config.h"
#include "remoting/protocol/token_validator.h"
#include "url/gurl.h"

namespace net {
class ClientCertStore;
}

namespace remoting {

class TokenValidatorBase
    : public net::URLRequest::Delegate,
      public protocol::TokenValidator {
 public:
  TokenValidatorBase(
      const ThirdPartyAuthConfig& third_party_auth_config,
      const std::string& token_scope,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);
  ~TokenValidatorBase() override;

  // TokenValidator interface.
  void ValidateThirdPartyToken(
      const std::string& token,
      const base::Callback<void(const std::string& shared_secret)>&
          on_token_validated) override;

  const GURL& token_url() const override;
  const std::string& token_scope() const override;

  // URLRequest::Delegate interface.
  void OnResponseStarted(net::URLRequest* source, int net_result) override;
  void OnReadCompleted(net::URLRequest* source, int net_result) override;
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnCertificateRequested(
      net::URLRequest* source,
      net::SSLCertRequestInfo* cert_request_info) override;

 protected:
  void OnCertificatesSelected(net::ClientCertStore* unused,
                              net::ClientCertIdentityList selected_certs);

  virtual void StartValidateRequest(const std::string& token) = 0;
  virtual void ContinueWithCertificate(
      scoped_refptr<net::X509Certificate> client_cert,
      scoped_refptr<net::SSLPrivateKey> client_private_key);
  virtual bool IsValidScope(const std::string& token_scope);
  std::string ProcessResponse(int net_result);

  // Constructor parameters.
  ThirdPartyAuthConfig third_party_auth_config_;
  std::string token_scope_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // URLRequest related fields.
  std::unique_ptr<net::URLRequest> request_;
  scoped_refptr<net::IOBuffer> buffer_;
  std::string data_;

  // This is set by OnReceivedRedirect() if the token validation request is
  // being re-submitted as a POST request. This can happen if the authentication
  // cookie has not yet been set, and a login handler redirection causes the
  // POST request to be turned into a GET operation, losing the POST data. In
  // this case, an immediate retry (with the same cookie jar) is expected to
  // succeeed.
  bool retrying_request_ = false;

  // Stores the most recently requested token, in case the validation request
  // needs to be retried.
  std::string token_;

  base::Callback<void(const std::string& shared_secret)> on_token_validated_;

  base::WeakPtrFactory<TokenValidatorBase> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TokenValidatorBase);
};

}  // namespace remoting

#endif  // REMOTING_HOST_TOKEN_VALIDATOR_BASE_H
