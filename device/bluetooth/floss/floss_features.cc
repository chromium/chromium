// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_features.h"

#include "base/system/sys_info.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace floss {
namespace features {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables Floss client if supported by platform
BASE_FEATURE(kFlossEnabled, "Floss", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFlossIsAvailable,
             "FlossIsAvailable",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFlossIsAvailabilityCheckNeeded,
             "FlossIsAvailabilityCheckNeeded",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLLPrivacyIsAvailable,
             "LLPrivacyIsAvailable",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {
const char* kNotLaunchedBoards[] = {
    // Chrome unittests have an empty board name.
    // TODO(b/369038879): Remove this after all unittests could pass with Floss.
    "",
    // Wave QCA
    "JACUZZI", "KUKUI", "STRONGBAD", "TROGDOR",
    // Wave RTL8822
    "ASURADA", "GRUNT", "ZORK",
    // Wave AC7265
    "EVE", "FIZZ", "KALISTA", "NAMI", "NAUTILUS", "NOCTURNE", "RAMMUS",
    "SORAKA", "CORAL", "PYRO", "REEF", "SAND", "SNAPPY",
    // Wave MVL8897
    "ELM", "HANA",
    // ChromeOS Flex
    "REVEN"};
}  // namespace

static bool IsDeviceLaunchedFloss() {
  std::string board = base::SysInfo::HardwareModelName();
  for (auto* b : kNotLaunchedBoards) {
    if (board.compare(b) == 0) {
      return false;
    }
  }

  return true;
}
#endif

bool IsFlossEnabled() {
  if (IsFlossAvailabilityCheckNeeded() && !IsFlossAvailable()) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Default to enable Floss if the feature is not overridden and the device is
  // launched.
  if (!base::FeatureList::GetStateIfOverridden(floss::features::kFlossEnabled)
           .has_value() &&
      IsDeviceLaunchedFloss()) {
    return true;
  }
  return base::FeatureList::IsEnabled(floss::features::kFlossEnabled);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->UseFlossBluetooth();
#else
  return false;
#endif
}

bool IsFlossAvailable() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(floss::features::kFlossIsAvailable);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsFlossAvailable();
#else
  return false;
#endif
}

bool IsFlossAvailabilityCheckNeeded() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(
      floss::features::kFlossIsAvailabilityCheckNeeded);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsFlossAvailabilityCheckNeeded();
#else
  return false;
#endif
}

bool IsLLPrivacyAvailable() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(floss::features::kLLPrivacyIsAvailable);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsLLPrivacyAvailable();
#else
  return false;
#endif
}
}  // namespace features
}  // namespace floss
