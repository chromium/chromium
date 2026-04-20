// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CRYPTO_PRIVATE_KEY_H_
#define NET_SSL_CRYPTO_PRIVATE_KEY_H_

#include "base/memory/scoped_refptr.h"
#include "crypto/keypair.h"
#include "net/base/net_export.h"

namespace net {

class SSLPrivateKey;

// Returns a new SSLPrivateKey which uses `key` for signing operations.
NET_EXPORT scoped_refptr<SSLPrivateKey> WrapCryptoPrivateKey(
    crypto::keypair::PrivateKey key);

}  // namespace net

#endif  // NET_SSL_CRYPTO_PRIVATE_KEY_H_
