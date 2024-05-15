// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "cdm_info.h"

#include "base/notreached.h"

namespace content {

std::string GetCdmInfoRobustnessName(CdmInfo::Robustness robustness) {
  switch (robustness) {
    case CdmInfo::Robustness::kHardwareSecure:
      return "Hardware Secure";
    case CdmInfo::Robustness::kSoftwareSecure:
      return "Software Secure";
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace content