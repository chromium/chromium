// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_HIT_TEST_SUBSCRIPTION_ID_H_
#define DEVICE_VR_PUBLIC_MOJOM_HIT_TEST_SUBSCRIPTION_ID_H_

#include "base/types/id_type.h"

namespace device {

using HitTestSubscriptionId = ::base::IdTypeU64<class HitTestSubscriptionIdTag>;
constexpr HitTestSubscriptionId kInvalidHitTestSubscriptionId;

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_HIT_TEST_SUBSCRIPTION_ID_H_
