// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_pool_additional_capacity.h"

#include "base/feature_list.h"
#include "net/base/features.h"

namespace net {

// static
SocketPoolAdditionalCapacity SocketPoolAdditionalCapacity::Create() {
  if (base::FeatureList::IsEnabled(
          features::kTcpSocketPoolLimitRandomization)) {
    return SocketPoolAdditionalCapacity(
        features::kTcpSocketPoolLimitRandomizationBase.Get(),
        features::kTcpSocketPoolLimitRandomizationCapacity.Get(),
        features::kTcpSocketPoolLimitRandomizationMinimum.Get(),
        features::kTcpSocketPoolLimitRandomizationNoise.Get());
  }
  return SocketPoolAdditionalCapacity();
}

// static
SocketPoolAdditionalCapacity SocketPoolAdditionalCapacity::CreateForTest(
    double base,
    int capacity,
    double minimum,
    double noise) {
  return SocketPoolAdditionalCapacity(base, capacity, minimum, noise);
}

}  // namespace net
