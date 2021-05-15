// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_CELLULAR_SIGNAL_STRENGTH_H_
#define NET_ANDROID_CELLULAR_SIGNAL_STRENGTH_H_

#include <jni.h>
#include <stdint.h>

#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace android {

namespace cellular_signal_strength {

// Returns the signal strength level (between 0 and 4, both inclusive) of the
// currently registered cellular connection. If the value is unavailable, an
// empty value is returned. If the signal strength value returned by platform
// API is less than 0, this method returns 0. If the platform API returns a
// value larger than 4, then this method returns 4.
NET_EXPORT_PRIVATE absl::optional<int32_t> GetSignalStrengthLevel();

}  // namespace cellular_signal_strength

}  // namespace android

}  // namespace net

#endif  // NET_ANDROID_CELLULAR_SIGNAL_STRENGTH_H_
