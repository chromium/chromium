// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLIENT_CERTIFICATE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_CLIENT_CERTIFICATE_DELEGATE_H_

namespace net {
class SSLPrivateKey;
class X509Certificate;
}

namespace content {

// A delegate interface for selecting a client certificate for use with a
// network request. If the delegate is destroyed without calling
// ContinueWithCertificate, the certificate request will be aborted.
class ClientCertificateDelegate {
 public:
  ClientCertificateDelegate() {}

  ClientCertificateDelegate(const ClientCertificateDelegate&) = delete;
  ClientCertificateDelegate& operator=(const ClientCertificateDelegate&) =
      delete;

  virtual ~ClientCertificateDelegate() {}

  // Continue the request with |cert| and matching |key|. |cert| may be nullptr
  // to continue without supplying a certificate. This decision will be
  // remembered for future requests to the domain.
  virtual void ContinueWithCertificate(
      scoped_refptr<net::X509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> key) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLIENT_CERTIFICATE_DELEGATE_H_
