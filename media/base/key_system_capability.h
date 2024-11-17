// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEM_CAPABILITY_H_
#define MEDIA_BASE_KEY_SYSTEM_CAPABILITY_H_

#include <optional>

#include "base/types/expected.h"
#include "media/base/cdm_capability.h"
#include "media/base/media_export.h"

namespace media {

struct MEDIA_EXPORT KeySystemCapability {
  KeySystemCapability();
  KeySystemCapability(CdmCapabilityOrStatus sw_cdm_capability_or_status,
                      CdmCapabilityOrStatus hw_cdm_capability_or_status);

  KeySystemCapability(const KeySystemCapability& other);
  ~KeySystemCapability();

  CdmCapabilityOrStatus sw_cdm_capability_or_status =
      base::unexpected(CdmCapabilityQueryStatus::kUnknown);
  CdmCapabilityOrStatus hw_cdm_capability_or_status =
      base::unexpected(CdmCapabilityQueryStatus::kUnknown);
};

bool MEDIA_EXPORT operator==(const KeySystemCapability& lhs,
                             const KeySystemCapability& rhs);

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEM_CAPABILITY_H_
