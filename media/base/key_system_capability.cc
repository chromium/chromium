// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_system_capability.h"

#include "media/base/cdm_capability.h"

namespace media {

KeySystemCapability::KeySystemCapability() = default;
KeySystemCapability::KeySystemCapability(
    std::optional<CdmCapability> sw_secure_capabilty,
    std::optional<CdmCapability> hw_secure_capabilty)
    : sw_secure_capability(std::move(sw_secure_capabilty)),
      hw_secure_capability(std::move(hw_secure_capabilty)) {}

KeySystemCapability::KeySystemCapability(const KeySystemCapability& other) =
    default;

KeySystemCapability::~KeySystemCapability() = default;

bool operator==(const KeySystemCapability& lhs,
                const KeySystemCapability& rhs) {
  return lhs.sw_secure_capability == rhs.sw_secure_capability &&
         lhs.hw_secure_capability == rhs.hw_secure_capability;
}

}  // namespace media
