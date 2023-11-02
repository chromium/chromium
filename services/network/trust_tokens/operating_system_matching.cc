// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/operating_system_matching.h"

#include "build/build_config.h"

namespace network {

bool IsCurrentOperatingSystem(mojom::TrustTokenKeyCommitmentResult::Os os) {
#if BUILDFLAG(IS_ANDROID)
  return os == mojom::TrustTokenKeyCommitmentResult::Os::kAndroid;
#else
  return false;
#endif
}

}  // namespace network
