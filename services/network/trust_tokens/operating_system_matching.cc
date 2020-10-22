// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/operating_system_matching.h"

#include "build/build_config.h"

namespace network {

bool IsCurrentOperatingSystem(mojom::TrustTokenKeyCommitmentResult::Os os) {
  switch (os) {
    case mojom::TrustTokenKeyCommitmentResult::Os::kAndroid:
#if defined(OS_ANDROID)
      return true;
#endif  // defined(OS_ANDROID)
      break;
  }

  return false;
}

}  // namespace network
