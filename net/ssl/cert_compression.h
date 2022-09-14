// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CERT_COMPRESSION_H_
#define NET_SSL_CERT_COMPRESSION_H_

#include "third_party/boringssl/src/include/openssl/base.h"

namespace net {

// Configures certificate compression callbacks on an SSL context.  The
// availability of individual algorithms may depend on the parameters with
// which the network stack is compiled.
void ConfigureCertificateCompression(SSL_CTX* ctx);

}  // namespace net

#endif  // NET_SSL_CERT_COMPRESSION_H_
