// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PRF_INPUT_H_
#define DEVICE_FIDO_PRF_INPUT_H_

#include <stdint.h>

#include <array>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "components/cbor/values.h"

namespace device {

// PRFInput contains salts for the hmac-secret or prf extension, potentially
// specific to a given credential ID.
struct COMPONENT_EXPORT(DEVICE_FIDO) PRFInput {
  PRFInput();
  PRFInput(const PRFInput&);
  PRFInput(PRFInput&&);
  PRFInput& operator=(const PRFInput&);
  ~PRFInput();

  static std::optional<PRFInput> FromCBOR(const cbor::Value& v);

  cbor::Value::MapValue ToCBOR() const;

  // Hashes the inputs into the corresponding salts.
  void HashInputsIntoSalts();

  // Evaluates the HMAC using the provided salts.
  static std::vector<uint8_t> EvaluateHMAC(
      base::span<const uint8_t> hmac_key,
      const std::array<uint8_t, 32>& hmac_salt1,
      const std::optional<std::array<uint8_t, 32>>& hmac_salt2);

  // Evaluates the HMAC using this input's salts.
  std::vector<uint8_t> EvaluateHMAC(base::span<const uint8_t> hmac_key) const;

  std::optional<std::vector<uint8_t>> credential_id;
  // Input values are provided both unhashed (as `input1` and `input2`) and
  // hashed (as `salt1` and `salt2`). Security keys use the hashed values but,
  // e.g., iCloud Keychain needs unhashed values.
  std::vector<uint8_t> input1;
  std::array<uint8_t, 32> salt1;
  std::optional<std::vector<uint8_t>> input2;
  std::optional<std::array<uint8_t, 32>> salt2;
};

}  // namespace device

#endif  // DEVICE_FIDO_PRF_INPUT_H_
