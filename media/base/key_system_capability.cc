// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_system_capability.h"

#include "media/base/cdm_capability.h"

namespace media {

KeySystemCapability::KeySystemCapability() = default;

KeySystemCapability::KeySystemCapability(
    CdmCapabilityOrStatus sw_cdm_capability_or_status,
    CdmCapabilityOrStatus hw_cdm_capability_or_status)
    : sw_cdm_capability_or_status(std::move(sw_cdm_capability_or_status)),
      hw_cdm_capability_or_status(std::move(hw_cdm_capability_or_status)) {}

KeySystemCapability::KeySystemCapability(const KeySystemCapability& other) =
    default;

KeySystemCapability::~KeySystemCapability() = default;

bool operator==(const KeySystemCapability& lhs,
                const KeySystemCapability& rhs) {
  return lhs.sw_cdm_capability_or_status == rhs.sw_cdm_capability_or_status &&
         lhs.hw_cdm_capability_or_status == rhs.hw_cdm_capability_or_status;
}

}  // namespace media
