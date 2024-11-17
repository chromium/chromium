// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/key_system_capability_mojom_traits.h"

#include "media/base/cdm_capability.h"

namespace mojo {

// static
bool StructTraits<media::mojom::KeySystemCapabilityDataView,
                  media::KeySystemCapability>::
    Read(media::mojom::KeySystemCapabilityDataView input,
         media::KeySystemCapability* output) {
  std::optional<media::CdmCapability> sw_secure_capability;
  if (!input.ReadSwSecureCapability(&sw_secure_capability)) {
    return false;
  }

  std::optional<media::CdmCapability> hw_secure_capability;
  if (!input.ReadHwSecureCapability(&hw_secure_capability)) {
    return false;
  }

  std::optional<media::CdmCapabilityQueryStatus>
      sw_secure_capability_query_status;
  if (!input.ReadSwSecureCapabilityQueryStatus(
          &sw_secure_capability_query_status)) {
    return false;
  }

  std::optional<media::CdmCapabilityQueryStatus>
      hw_secure_capability_query_status;
  if (!input.ReadHwSecureCapabilityQueryStatus(
          &hw_secure_capability_query_status)) {
    return false;
  }

  if ((sw_secure_capability.has_value() &&
       sw_secure_capability_query_status.has_value()) ||
      (!sw_secure_capability.has_value() &&
       !sw_secure_capability_query_status.has_value())) {
    return false;
  }

  if ((hw_secure_capability.has_value() &&
       hw_secure_capability_query_status.has_value()) ||
      (!hw_secure_capability.has_value() &&
       !hw_secure_capability_query_status.has_value())) {
    return false;
  }

  media::KeySystemCapability key_system_capability;
  if (sw_secure_capability.has_value()) {
    key_system_capability.sw_cdm_capability_or_status =
        sw_secure_capability.value();
  } else if (sw_secure_capability_query_status.has_value()) {
    key_system_capability.sw_cdm_capability_or_status =
        base::unexpected(sw_secure_capability_query_status.value());
  }
  if (hw_secure_capability.has_value()) {
    key_system_capability.hw_cdm_capability_or_status =
        hw_secure_capability.value();
  } else if (hw_secure_capability_query_status.has_value()) {
    key_system_capability.hw_cdm_capability_or_status =
        base::unexpected(hw_secure_capability_query_status.value());
  }

  *output = key_system_capability;
  return true;
}

}  // namespace mojo
