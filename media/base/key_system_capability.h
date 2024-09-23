// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEM_CAPABILITY_H_
#define MEDIA_BASE_KEY_SYSTEM_CAPABILITY_H_

#include "media/base/cdm_capability.h"
#include "media/base/media_export.h"

namespace media {

struct MEDIA_EXPORT KeySystemCapability {
  KeySystemCapability();
  KeySystemCapability(std::optional<CdmCapability> sw_secure_capability,
                      std::optional<CdmCapability> hw_secure_capability);

  KeySystemCapability(const KeySystemCapability& other);
  ~KeySystemCapability();

  std::optional<CdmCapability> sw_secure_capability;
  std::optional<CdmCapability> hw_secure_capability;
};

bool MEDIA_EXPORT operator==(const KeySystemCapability& lhs,
                             const KeySystemCapability& rhs);

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEM_CAPABILITY_H_
