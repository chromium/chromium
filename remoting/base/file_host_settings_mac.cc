// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/file_host_settings.h"

namespace remoting {

namespace {
// Note: If this path is changed, also update the value set in:
// //remoting/host/mac/constants_mac.cc
const char kHostSettingsFilePath[] =
    "/Library/PrivilegedHelperTools/org.chromium.chromoting.settings.json";
}  // namespace

base::FilePath FileHostSettings::GetSettingsFilePath() {
  return base::FilePath(kHostSettingsFilePath);
}

}  // namespace remoting
