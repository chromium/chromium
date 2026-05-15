// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/public/cpp/bluetooth_features.h"

namespace features {

// When enabled, calling navigator.bluetooth.getAvailability() does not prevent
// the frame from entering the back forward cache.
BASE_FEATURE(kWebBluetoothAllowGetAvailabilityWithBfcache,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, BluetoothSocketMac::Send pre-calculates the number of chunks
// to write and initializes request_ptr->active_async_writes to this total
// upfront. This prevents a Use-After-Free caused by premature cleanup if
// completion callbacks fire synchronously during a multi-chunk write. Without
// this, a synchronous callback would decrement the counter and could pop the
// send_queue_ before all chunks have been issued.
BASE_FEATURE(kBluetoothSocketMacPreCalculateWriteChunks,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
