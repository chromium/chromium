// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CLIENT_CERT_TYPE_H_
#define NET_SSL_SSL_CLIENT_CERT_TYPE_H_

namespace net {

// TLS ClientCertificateType Identifiers
// http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-1
// Certificate types not listed in this enum will be ignored.
enum class SSLClientCertType {
  kRsaSign = 1,
  kEcdsaSign = 64,
  kMaxValue = kEcdsaSign,
};

}  // namespace net

#endif  // NET_SSL_SSL_CLIENT_CERT_TYPE_H_
