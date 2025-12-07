// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_features.h"

#include "base/system/sys_info.h"

namespace floss {
namespace features {

#if BUILDFLAG(IS_CHROMEOS)
// Enables Floss client if supported by platform
BASE_FEATURE(kFlossEnabled, "Floss", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kLLPrivacyIsAvailable, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {
constexpr const char* kNotLaunchedBoards[] = {
    // Chrome unittests have an empty board name.
    // TODO(b/369038879): Remove this after all unittests could pass with Floss.
    "",
};
}  // namespace

static bool IsDeviceLaunchedFloss() {
  std::string board = base::SysInfo::HardwareModelName();
  // Ignore the parts after the first dash, i.e., treat variant boards the same.
  if (auto pos = board.find('-'); pos != std::string::npos) {
    board.erase(pos);
  }
  for (auto* b : kNotLaunchedBoards) {
    if (board.compare(b) == 0) {
      return false;
    }
  }

  return true;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsFlossEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  // Default to enable Floss if the feature is not overridden and the device is
  // launched.
  if (!base::FeatureList::GetStateIfOverridden(floss::features::kFlossEnabled)
           .has_value() &&
      IsDeviceLaunchedFloss()) {
    return true;
  }
  return base::FeatureList::IsEnabled(floss::features::kFlossEnabled);
#else
  return false;
#endif
}

bool IsLLPrivacyAvailable() {
#if BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(floss::features::kLLPrivacyIsAvailable);
#else
  return false;
#endif
}
}  // namespace features
}  // namespace floss
