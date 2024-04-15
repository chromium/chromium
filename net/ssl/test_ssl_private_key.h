// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_TEST_SSL_PRIVATE_KEY_H_
#define NET_SSL_TEST_SSL_PRIVATE_KEY_H_

#include "base/memory/scoped_refptr.h"

namespace crypto {
class RSAPrivateKey;
}

namespace net {

class SSLPrivateKey;

// Returns a new SSLPrivateKey which uses |key| for signing operations or
// nullptr on error.
scoped_refptr<SSLPrivateKey> WrapRSAPrivateKey(
    crypto::RSAPrivateKey* rsa_private_key);
scoped_refptr<SSLPrivateKey> CreateFailSigningSSLPrivateKey();

}  // namespace net

#endif  // NET_SSL_TEST_SSL_PRIVATE_KEY_H_
