// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_features.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace floss {
namespace features {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables Floss client if supported by platform
BASE_FEATURE(kFlossEnabled, "Floss", base::FEATURE_DISABLED_BY_DEFAULT);
#endif

bool IsFlossEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(floss::features::kFlossEnabled);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->UseFlossBluetooth();
#else
  return false;
#endif
}

}  // namespace features
}  // namespace floss
