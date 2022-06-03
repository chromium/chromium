// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_TYPE_H_

#include <stdint.h>

#include "services/device/public/mojom/wake_lock.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace WTF {

class String;

}  // namespace WTF

namespace blink {

// This header contains types and utility functions for representing the
// WakeLockType enum as a C++ enum, and for converting between WakeLockType and
// device.mojom.WakeLockType.

// https://w3c.github.io/screen-wake-lock/#the-wakelocktype-enum
enum class WakeLockType : int8_t { kScreen, kSystem, kMaxValue = kSystem };

// Useful for creating arrays with size N, where N is the number of different
// wake lock types.
constexpr size_t kWakeLockTypeCount =
    static_cast<size_t>(WakeLockType::kMaxValue) + 1;

MODULES_EXPORT device::mojom::blink::WakeLockType ToMojomWakeLockType(
    WakeLockType type);

MODULES_EXPORT WakeLockType ToWakeLockType(const String& type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_TYPE_H_
