// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_preferences.h"

#include "base/base64.h"
#include "gpu/ipc/common/gpu_preferences.mojom.h"

namespace gpu {

GpuPreferences::GpuPreferences() = default;

GpuPreferences::GpuPreferences(const GpuPreferences& other) = default;

GpuPreferences::~GpuPreferences() = default;

std::string GpuPreferences::ToSwitchValue() {
  std::vector<uint8_t> serialized = gpu::mojom::GpuPreferences::Serialize(this);

  std::string encoded;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(serialized.data()),
                        serialized.size()),
      &encoded);
  return encoded;
}

bool GpuPreferences::FromSwitchValue(const std::string& data) {
  std::string decoded;
  if (!base::Base64Decode(data, &decoded))
    return false;
  if (!gpu::mojom::GpuPreferences::Deserialize(decoded.data(), decoded.size(),
                                               this)) {
    return false;
  }
  return true;
}

}  // namespace gpu
