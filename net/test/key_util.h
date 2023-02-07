// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_KEY_UTIL_H_
#define NET_TEST_KEY_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace base {
class FilePath;
}

namespace net {

class SSLPrivateKey;

namespace key_util {

// Loads a PEM-encoded private key file from |filepath| into an EVP_PKEY object.
// Returns the new EVP_PKEY or nullptr on error.
bssl::UniquePtr<EVP_PKEY> LoadEVP_PKEYFromPEM(const base::FilePath& filepath);

// Returns a PEM-encoded string representing |key|.
std::string PEMFromPrivateKey(EVP_PKEY* key);

// Loads a PEM-encoded private key file into a SSLPrivateKey object.
// |filepath| is the private key file path.
// Returns the new SSLPrivateKey.
scoped_refptr<SSLPrivateKey> LoadPrivateKeyOpenSSL(
    const base::FilePath& filepath);

}  // namespace key_util

}  // namespace net

#endif  // NET_TEST_KEY_UTIL_H_
