// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CLIENT_CERT_VERIFIER_H_
#define NET_CERT_CLIENT_CERT_VERIFIER_H_

#include <memory>

#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"

namespace net {

class X509Certificate;

// ClientCertVerifier represents a service for verifying certificates.
class NET_EXPORT ClientCertVerifier {
 public:
  class Request {
   public:
    Request() = default;

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    // Destruction of the Request cancels it.
    virtual ~Request() = default;
  };

  virtual ~ClientCertVerifier() = default;

  // Verifies the given certificate as a client certificate.
  // Returns OK if successful or an error code upon failure.
  virtual int Verify(X509Certificate* cert,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* out_req) = 0;
};

}  // namespace net

#endif  // NET_CERT_CLIENT_CERT_VERIFIER_H_
