// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_AUTH_CERT_OBSERVER_H_
#define SERVICES_NETWORK_TEST_TEST_AUTH_CERT_OBSERVER_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/auth_and_certificate_observer.mojom.h"

namespace network {

// A helper class with a basic AuthenticationAndCertificateObserver
// implementation for use in unittests, so they can just override the parts they
// need.
class TestAuthCertObserver
    : public mojom::AuthenticationAndCertificateObserver {
 public:
  TestAuthCertObserver();
  ~TestAuthCertObserver() override;

  mojo::PendingRemote<mojom::AuthenticationAndCertificateObserver> Bind();

  void set_ignore_certificate_errors(bool ignore_certificate_errors) {
    ignore_certificate_errors_ = ignore_certificate_errors;
  }

  // mojom::AuthenticationAndCertificateObserver overrides:
  void OnSSLCertificateError(const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCertificateRequested(
      const base::Optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<mojom::ClientCertificateResponder>
          client_cert_responder) override;
  void OnAuthRequired(
      const base::Optional<base::UnguessableToken>& window_id,
      uint32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void Clone(mojo::PendingReceiver<AuthenticationAndCertificateObserver>
                 observer) override;

 private:
  mojo::ReceiverSet<mojom::AuthenticationAndCertificateObserver> receivers_;
  bool ignore_certificate_errors_ = false;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_AUTH_CERT_OBSERVER_H_
