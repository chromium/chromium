// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_MOCK_CLIENT_CERT_VERIFIER_H_
#define NET_CERT_MOCK_CLIENT_CERT_VERIFIER_H_

#include <list>
#include <memory>

#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/cert/client_cert_verifier.h"

namespace net {

class MockClientCertVerifier : public ClientCertVerifier {
 public:
  // Creates a new MockClientCertVerifier. By default, any call to Verify() will
  // result in the cert status being flagged as CERT_STATUS_INVALID and return
  // an ERR_CERT_INVALID network error code. This behaviour can be overridden
  // by calling set_default_result() to change the default return value for
  // Verify() or by calling one of the AddResult*() methods to specifically
  // handle a certificate or certificate and host.
  MockClientCertVerifier();

  ~MockClientCertVerifier() override;

  // ClientCertVerifier implementation
  int Verify(X509Certificate* cert,
             CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req) override;

  // Sets the default return value for Verify() for certificates/hosts that do
  // not have explicit results added via the AddResult*() methods.
  void set_default_result(int default_result) {
    default_result_ = default_result;
  }

  // Adds a rule that will cause any call to Verify() for |cert| to return rv.
  // Note: Only the primary certificate of |cert| is checked. Any intermediate
  // certificates will be ignored.
  void AddResultForCert(X509Certificate* cert, int rv);

 private:
  struct Rule;
  typedef std::list<Rule> RuleList;

  int default_result_ = ERR_CERT_INVALID;
  RuleList rules_;
};

}  // namespace net

#endif  // NET_CERT_MOCK_CLIENT_CERT_VERIFIER_H_
