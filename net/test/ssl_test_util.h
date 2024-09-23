// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_SSL_TEST_UTIL_H_
#define NET_TEST_SSL_TEST_UTIL_H_

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

// Generates new ECH keys and ECHConfig with the specified public name and
// maximum name length. Returns an `SSL_ECH_KEYS` structure on success or
// `nullptr` on error. On success, sets `*ech_config_list` to an ECHConfigList
// containing the generated ECHConfig.
bssl::UniquePtr<SSL_ECH_KEYS> MakeTestEchKeys(
    std::string_view public_name,
    size_t max_name_len,
    std::vector<uint8_t>* ech_config_list);

}  // namespace net

#endif  // NET_TEST_SSL_TEST_UTIL_H_
