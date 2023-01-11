// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_ANDROID_H_
#define CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_ANDROID_H_

#include <string>

#include "base/functional/callback.h"
#include "content/public/common/cdm_info.h"
#include "media/cdm/cdm_capability.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Calls `cdm_capability_cb` with the CdmCapability supported on Android for
// `key_system` with robustness `robustness`. Capability will be base::nullopt
// if the device does not support `key_system` and `robustness`.
void GetAndroidCdmCapability(const std::string& key_system,
                             CdmInfo::Robustness robustness,
                             media::CdmCapabilityCB cdm_capability_cb);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_ANDROID_H_
