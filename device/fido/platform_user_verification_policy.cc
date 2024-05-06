// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/platform_user_verification_policy.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/util.h"
#endif

namespace device::fido {

bool PlatformWillDoUserVerification(UserVerificationRequirement requirement) {
#if BUILDFLAG(IS_WIN)
  return true;
#elif BUILDFLAG(IS_MAC)
  return requirement == UserVerificationRequirement::kRequired ||
         mac::DeviceHasBiometricsAvailable();
#else
  // This default is for unit tests.
  return true;
#endif
}

}  // namespace device::fido
