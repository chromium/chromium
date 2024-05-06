// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_HASH_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_HASH_H_

#include <cstdint>
#include <vector>

#include "base/component_export.h"
#include "base/json/json_value_converter.h"

namespace device::enclave {

enum HashType {
  kSHA256,
};

struct COMPONENT_EXPORT(DEVICE_FIDO) Hash {
  Hash(std::vector<uint8_t> bytes, HashType hash_type);
  Hash();
  ~Hash();
  Hash(const Hash& hash);

  static void RegisterJSONConverter(base::JSONValueConverter<Hash>* converter);

  std::vector<uint8_t> bytes;
  HashType hash_type = kSHA256;
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_HASH_H_
