// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CURTAIN_MODE_CHROMEOS_H_
#define REMOTING_HOST_CURTAIN_MODE_CHROMEOS_H_

#include "remoting/host/curtain_mode.h"

namespace remoting {

// Helper class that handles everything related to curtained sessions on
// ChromeOS, which includes:
//    - Creating a virtual display
//    - Installing the curtain screen
//    - Suppressing local input
class CurtainModeChromeOs : public CurtainMode {
 public:
  CurtainModeChromeOs() = default;
  CurtainModeChromeOs(const CurtainModeChromeOs&) = delete;
  CurtainModeChromeOs& operator=(const CurtainModeChromeOs&) = delete;
  ~CurtainModeChromeOs() override = default;

  // CurtainMode implementation:
  bool Activate() override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CURTAIN_MODE_CHROMEOS_H_
