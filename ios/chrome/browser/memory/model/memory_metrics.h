// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MEMORY_MODEL_MEMORY_METRICS_H_
#define IOS_CHROME_BROWSER_MEMORY_MODEL_MEMORY_METRICS_H_

#include <stdint.h>

namespace memory_util {
// "Physical Free" memory metric. This corresponds to the "Physical Memory Free"
// value reported by the Memory Monitor in Instruments.
uint64_t GetFreePhysicalBytes();

// "Real Memory Used" memory metric. This corresponds to the "Real Memory" value
// reported for the app by the Memory Monitor in Instruments.
uint64_t GetRealMemoryUsedInBytes();

// "Xcode Gauge" memory metric. This corresponds to the "Memory" value reported
// for the app by the Debug Navigator in Xcode. Only supported in iOS 7 and
// later.
uint64_t GetInternalVMBytes();

// "Dirty VM" memory metric. This corresponds to the "Dirty Size" value reported
// for the app by the VM Tracker in Instruments.
uint64_t GetDirtyVMBytes();
}  // namespace memory_util

#endif  // IOS_CHROME_BROWSER_MEMORY_MODEL_MEMORY_METRICS_H_
