// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CLIENT_CERT_TYPE_H_
#define NET_SSL_SSL_CLIENT_CERT_TYPE_H_

namespace net {

// TLS ClientCertificateType Identifiers
// http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-1
enum SSLClientCertType {
  CLIENT_CERT_RSA_SIGN = 1,
  CLIENT_CERT_ECDSA_SIGN = 64,
  // 224-255 are Reserved for Private Use, we pick one to use as "invalid".
  CLIENT_CERT_INVALID_TYPE = 255,
};

}  // namespace net

#endif  // NET_SSL_SSL_CLIENT_CERT_TYPE_H_
