// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "net/cert/pki/crl.h"
#include "net/der/input.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const net::der::Input input_der(data, size);

  uint8_t data_hash[SHA256_DIGEST_LENGTH];
  SHA256(data, size, data_hash);
  const net::CrlVersion crl_version =
      (data_hash[0] % 2) ? net::CrlVersion::V2 : net::CrlVersion::V1;
  const size_t serial_len = data_hash[1] % (sizeof(data_hash) - 2);
  assert(serial_len + 2 < sizeof(data_hash));
  const net::der::Input cert_serial(
      reinterpret_cast<const uint8_t*>(data_hash + 2), serial_len);

  net::GetCRLStatusForCert(cert_serial, crl_version,
                           absl::make_optional(input_der));

  return 0;
}
