// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"

#include "base/notreached.h"
#include "services/device/public/mojom/wake_lock.mojom-blink.h"

namespace blink {

device::mojom::blink::WakeLockType ToMojomWakeLockType(
    V8WakeLockType::Enum type) {
  switch (type) {
    case V8WakeLockType::Enum::kScreen:
      return device::mojom::blink::WakeLockType::kPreventDisplaySleep;
    case V8WakeLockType::Enum::kSystem:
      return device::mojom::blink::WakeLockType::kPreventAppSuspension;
  }
}

}  // namespace blink
