// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_version.h"

namespace floss::version {

// Supported version range of the Floss API exported by Floss daemon.
constexpr char kMinimumSupportedFlossVerStr[] = "0.0";
constexpr char kMaximumSupportedFlossVerStr[] = "0.7";

base::Version IntoVersion(uint32_t version) {
  return base::Version({GetMajorVersion(version), GetMinorVersion(version)});
}

uint32_t GetMajorVersion(uint32_t version) {
  return (version >> 16);
}

uint32_t GetMinorVersion(uint32_t version) {
  return (version & 0xffff);
}

base::Version GetMinimalSupportedVersion() {
  return base::Version(kMinimumSupportedFlossVerStr);
}

base::Version GetMaximalSupportedVersion() {
  return base::Version(kMaximumSupportedFlossVerStr);
}

}  // namespace floss::version
