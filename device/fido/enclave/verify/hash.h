// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_HASH_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_HASH_H_

#include <cstdint>
#include <vector>

namespace device::enclave {

enum HashType {
  kSHA256,
};

struct Hash {
  Hash(std::vector<uint8_t> bytes, HashType hash_type);
  Hash();
  ~Hash();

  std::vector<uint8_t> bytes;
  HashType hash_type = kSHA256;
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_HASH_H_
