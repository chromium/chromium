// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_OPENSSL_PRIVATE_KEY_H_
#define NET_SSL_OPENSSL_PRIVATE_KEY_H_

#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace net {

class SSLPrivateKey;

// Returns a new SSLPrivateKey which uses `key` for signing operations.
NET_EXPORT scoped_refptr<SSLPrivateKey> WrapOpenSSLPrivateKey(
    bssl::UniquePtr<EVP_PKEY> key);

}  // namespace net

#endif  // NET_SSL_OPENSSL_PRIVATE_KEY_H_
