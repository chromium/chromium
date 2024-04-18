// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/hash.h"

#include <vector>

namespace device::enclave {

Hash::Hash(std::vector<uint8_t> bytes, HashType hash_type)
    : bytes(std::move(bytes)), hash_type(hash_type) {}
Hash::Hash() = default;
Hash::~Hash() = default;

}  // namespace device::enclave
