// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/utils/mac_utils.h"
#include "base/strings/stringprintf.h"

namespace device {

std::string HexErrorCode(IOReturn error_code) {
  return base::StringPrintf("0x%04x", error_code);
}

}  // namespace device
