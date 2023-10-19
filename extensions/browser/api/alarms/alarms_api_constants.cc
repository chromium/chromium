// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/alarms/alarms_api_constants.h"

namespace extensions {
namespace alarms_api_constants {

base::TimeDelta GetMinimumDelay(bool is_unpacked, int manifest_version) {
  if (is_unpacked) {
    return alarms_api_constants::kDevDelayMinimum;
  }
  if (manifest_version >= 3) {
    return alarms_api_constants::kMV3ReleaseDelayMinimum;
  }
  return alarms_api_constants::kMV2ReleaseDelayMinimum;
}

}  // namespace alarms_api_constants
}  // namespace extensions
