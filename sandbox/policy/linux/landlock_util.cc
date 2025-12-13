// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/landlock_util.h"

#include <errno.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/system_headers/linux_landlock.h"

namespace sandbox::policy {

namespace {

// The state of Landlock support on the system.
// Used to report through UMA.
enum LandlockState {
  kEnabled = 0,
  kDisabled = 1,
  kNotSupported = 2,
  kUnknown = 3,
  kMaxValue = kUnknown,
};

}  // namespace

void ReportLandlockStatus() {
  LandlockState landlock_state = LandlockState::kUnknown;
  const int landlock_version =
      landlock_create_ruleset(nullptr, 0, LANDLOCK_CREATE_RULESET_VERSION);
  if (landlock_version <= 0) {
    const int err = errno;
    switch (err) {
      case ENOSYS: {
        DVLOG(1) << "Landlock not supported by the kernel.";
        landlock_state = LandlockState::kNotSupported;
        break;
      }
      case EOPNOTSUPP: {
        DVLOG(1) << "Landlock supported by the kernel but disabled.";
        landlock_state = LandlockState::kDisabled;
        break;
      }
      default: {
        DVLOG(1) << "Could not determine Landlock state.";
        landlock_state = LandlockState::kUnknown;
      }
    }
  } else {
    DVLOG(1) << "Landlock enabled; Version " << landlock_version;
    landlock_state = LandlockState::kEnabled;
  }

  UMA_HISTOGRAM_ENUMERATION("Security.Sandbox.LandlockState", landlock_state);
}

}  // namespace sandbox::policy
