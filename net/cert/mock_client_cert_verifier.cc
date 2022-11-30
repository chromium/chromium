// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/mock_client_cert_verifier.h"

#include <memory>

#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"

namespace net {

struct MockClientCertVerifier::Rule {
  Rule(X509Certificate* cert, int rv) : cert(cert), rv(rv) { DCHECK(cert); }

  scoped_refptr<X509Certificate> cert;
  int rv;
};

MockClientCertVerifier::MockClientCertVerifier() = default;

MockClientCertVerifier::~MockClientCertVerifier() = default;

int MockClientCertVerifier::Verify(X509Certificate* cert,
                                   CompletionOnceCallback callback,
                                   std::unique_ptr<Request>* out_req) {
  for (const Rule& rule : rules_) {
    // Check just the client cert. Intermediates will be ignored.
    if (rule.cert->EqualsExcludingChain(cert))
      return rule.rv;
  }
  return default_result_;
}

void MockClientCertVerifier::AddResultForCert(X509Certificate* cert, int rv) {
  Rule rule(cert, rv);
  rules_.push_back(rule);
}

}  // namespace net
