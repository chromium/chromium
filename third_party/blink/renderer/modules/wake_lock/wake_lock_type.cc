// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

device::mojom::blink::WakeLockType ToMojomWakeLockType(WakeLockType type) {
  switch (type) {
    case WakeLockType::kScreen:
      return device::mojom::blink::WakeLockType::kPreventDisplaySleep;
    case WakeLockType::kSystem:
      return device::mojom::blink::WakeLockType::kPreventAppSuspension;
    default:
      NOTREACHED();
      return device::mojom::blink::WakeLockType::kMaxValue;
  }
}

WakeLockType ToWakeLockType(const String& type) {
  WakeLockType wake_lock_type;
  if (type == "screen") {
    wake_lock_type = WakeLockType::kScreen;
  } else if (type == "system") {
    wake_lock_type = WakeLockType::kSystem;
  } else {
    NOTREACHED();
    wake_lock_type = WakeLockType::kMaxValue;
  }
  return wake_lock_type;
}

}  // namespace blink
