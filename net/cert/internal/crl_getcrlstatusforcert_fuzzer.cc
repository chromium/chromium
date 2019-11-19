// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "crypto/sha2.h"
#include "net/cert/internal/crl.h"
#include "net/der/input.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const net::der::Input input_der(data, size);

  const std::string data_hash =
      crypto::SHA256HashString(input_der.AsStringPiece());
  const net::CrlVersion crl_version =
      (data_hash[0] % 2) ? net::CrlVersion::V2 : net::CrlVersion::V1;
  const size_t serial_len = data_hash[1] % (data_hash.size() - 2);
  CHECK_LT(serial_len + 2, data_hash.size());
  const net::der::Input cert_serial(
      reinterpret_cast<const uint8_t*>(data_hash.data() + 2), serial_len);

  net::GetCRLStatusForCert(cert_serial, crl_version,
                           base::make_optional(input_der));

  return 0;
}
