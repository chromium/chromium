// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_PRIVATE_KEY_TEST_UTIL_H_
#define NET_SSL_SSL_PRIVATE_KEY_TEST_UTIL_H_

#include <string>

#include "base/containers/span.h"

namespace net {

class SSLPrivateKey;

// Tests that |key| matches the private key serialized in |pkcs8|. It checks the
// reported type and key size are correct, and then it tests all advertised
// signature algorithms align with |pkcs8|. It does not test unadvertised
// algorithms, so the caller must check this list is as expected.
void TestSSLPrivateKeyMatches(SSLPrivateKey* key, std::string_view pkcs8);
void TestSSLPrivateKeyMatches(SSLPrivateKey* key,
                              base::span<const uint8_t> pkcs8);

}  // namespace net

#endif  // NET_SSL_SSL_PRIVATE_KEY_TEST_UTIL_H_
