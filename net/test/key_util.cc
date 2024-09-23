// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/key_util.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "net/ssl/openssl_private_key.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/pem.h"

namespace net::key_util {

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

std::string PEMFromPrivateKey(EVP_PKEY* key) {
  bssl::UniquePtr<BIO> temp_memory_bio(BIO_new(BIO_s_mem()));
  if (!temp_memory_bio) {
    LOG(ERROR) << "Failed to allocate temporary memory bio";
    return std::string();
  }
  if (!PEM_write_bio_PrivateKey(temp_memory_bio.get(), key, nullptr, nullptr, 0,
                                nullptr, nullptr)) {
    LOG(ERROR) << "Failed to write private key";
    return std::string();
  }
  const uint8_t* buffer;
  size_t len;
  if (!BIO_mem_contents(temp_memory_bio.get(), &buffer, &len)) {
    LOG(ERROR) << "BIO_mem_contents failed";
    return std::string();
  }
  return std::string(reinterpret_cast<const char*>(buffer), len);
}

scoped_refptr<SSLPrivateKey> LoadPrivateKeyOpenSSL(
    const base::FilePath& filepath) {
  bssl::UniquePtr<EVP_PKEY> key = LoadEVP_PKEYFromPEM(filepath);
  if (!key)
    return nullptr;
  return WrapOpenSSLPrivateKey(std::move(key));
}

}  // namespace net::key_util
