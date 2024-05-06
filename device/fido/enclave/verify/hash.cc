// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/hash.h"

#include <vector>

#include "base/json/json_value_converter.h"

namespace device::enclave {

namespace {

bool ConvertToBytes(const base::Value* s, std::vector<uint8_t>* result) {
  if (!s->is_string()) {
    return false;
  }
  std::vector<uint8_t> output(s->GetString().begin(), s->GetString().end());
  *result = std::move(output);
  return true;
}

bool CheckHashType(const base::Value* s, HashType* result) {
  if (!s->is_string()) {
    return false;
  }
  if (s->GetString() != "sha256") {
    return false;
  }
  *result = kSHA256;
  return true;
}

}  // namespace

Hash::Hash(std::vector<uint8_t> bytes, HashType hash_type)
    : bytes(std::move(bytes)), hash_type(hash_type) {}
Hash::Hash() = default;
Hash::~Hash() = default;
Hash::Hash(const Hash& hash) = default;

void Hash::RegisterJSONConverter(base::JSONValueConverter<Hash>* converter) {
  converter->RegisterCustomValueField("algorithm", &Hash::hash_type,
                                      &CheckHashType);
  converter->RegisterCustomValueField("value", &Hash::bytes, &ConvertToBytes);
}

}  // namespace device::enclave
