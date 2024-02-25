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

  std::optional<std::vector<uint8_t>> credential_id;
  std::array<uint8_t, 32> salt1;
  std::optional<std::array<uint8_t, 32>> salt2;
};

}  // namespace device

#endif  // DEVICE_FIDO_PRF_INPUT_H_
