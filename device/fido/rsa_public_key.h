// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_RSA_PUBLIC_KEY_H_
#define DEVICE_FIDO_RSA_PUBLIC_KEY_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "components/cbor/values.h"
#include "device/fido/public_key.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) RSAPublicKey {
 public:
  static std::unique_ptr<PublicKey> ExtractFromCOSEKey(
      int32_t algorithm,
      base::span<const uint8_t> cbor_bytes,
      const cbor::Value::MapValue& map);
};

}  // namespace device

#endif  // DEVICE_FIDO_RSA_PUBLIC_KEY_H_
