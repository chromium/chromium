// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/test/ssl_test_util.h"

#include <string>
#include <string_view>

#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace net {

bssl::UniquePtr<SSL_ECH_KEYS> MakeTestEchKeys(
    std::string_view public_name,
    size_t max_name_len,
    std::vector<uint8_t>* ech_config_list) {
  bssl::ScopedEVP_HPKE_KEY key;
  if (!EVP_HPKE_KEY_generate(key.get(), EVP_hpke_x25519_hkdf_sha256())) {
    return nullptr;
  }

  uint8_t* ech_config;
  size_t ech_config_len;
  if (!SSL_marshal_ech_config(&ech_config, &ech_config_len,
                              /*config_id=*/1, key.get(),
                              std::string(public_name).c_str(), max_name_len)) {
    return nullptr;
  }
  bssl::UniquePtr<uint8_t> scoped_ech_config(ech_config);

  uint8_t* ech_config_list_raw;
  size_t ech_config_list_len;
  bssl::UniquePtr<SSL_ECH_KEYS> keys(SSL_ECH_KEYS_new());
  if (!keys ||
      !SSL_ECH_KEYS_add(keys.get(), /*is_retry_config=*/1, ech_config,
                        ech_config_len, key.get()) ||
      !SSL_ECH_KEYS_marshal_retry_configs(keys.get(), &ech_config_list_raw,
                                          &ech_config_list_len)) {
    return nullptr;
  }
  bssl::UniquePtr<uint8_t> scoped_ech_config_list(ech_config_list_raw);

  ech_config_list->assign(ech_config_list_raw,
                          ech_config_list_raw + ech_config_list_len);
  return keys;
}

}  // namespace net
