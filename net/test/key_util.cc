// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/key_util.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/test_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/pem.h"

namespace net {

namespace key_util {

bssl::UniquePtr<EVP_PKEY> LoadEVP_PKEYFromPEM(const base::FilePath& filepath) {
  std::string data;
  if (!base::ReadFileToString(filepath, &data)) {
    LOG(ERROR) << "Could not read private key file: " << filepath.value();
    return nullptr;
  }
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(const_cast<char*>(data.data()),
                                           static_cast<int>(data.size())));
  if (!bio) {
    LOG(ERROR) << "Could not allocate BIO for buffer?";
    return nullptr;
  }
  bssl::UniquePtr<EVP_PKEY> result(
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
  if (!result) {
    LOG(ERROR) << "Could not decode private key file: " << filepath.value();
    return nullptr;
  }
  return result;
}

scoped_refptr<SSLPrivateKey> LoadPrivateKeyOpenSSL(
    const base::FilePath& filepath) {
  bssl::UniquePtr<EVP_PKEY> key = LoadEVP_PKEYFromPEM(filepath);
  if (!key)
    return nullptr;
  return WrapOpenSSLPrivateKey(std::move(key));
}

}  // namespace key_util

}  // namespace net
