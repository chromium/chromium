// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_TRAFFIC_STATS_H_
#define NET_ANDROID_TRAFFIC_STATS_H_

// This file provides functions that interact with TrafficStats APIs that are
// provided on Android.

#include <jni.h>
#include <stdint.h>

#include "net/base/net_export.h"

namespace net::android::traffic_stats {

// Returns true if the number of bytes transmitted since device boot is
// available and sets |*bytes| to that value. Counts packets across all network
// interfaces, and always increases monotonically since device boot.
// Statistics are measured at the network layer, so they include both TCP and
// UDP usage. |bytes| must not be nullptr.
NET_EXPORT bool GetTotalTxBytes(int64_t* bytes);

// Returns true if the number of bytes received since device boot is
// available and sets |*bytes| to that value. Counts packets across all network
// interfaces, and always increases monotonically since device boot.
// Statistics are measured at the network layer, so they include both TCP and
// UDP usage. |bytes| must not be nullptr.
NET_EXPORT bool GetTotalRxBytes(int64_t* bytes);

// Returns true if the number of bytes attributed to caller's UID since device
// boot are available and sets |*bytes| to that value. Counts packets across
// all network interfaces, and always increases monotonically since device
// boot. Statistics are measured at the network layer, so they include both TCP
// and UDP usage. |bytes| must not be nullptr.
NET_EXPORT bool GetCurrentUidTxBytes(int64_t* bytes);

// Returns true if the number of bytes attributed to caller's UID since device
// boot are available and sets |*bytes| to that value. Counts packets across
// all network interfaces, and always increases monotonically since device
// boot. Statistics are measured at the network layer, so they include both TCP
// and UDP usage. |bytes| must not be nullptr.
NET_EXPORT bool GetCurrentUidRxBytes(int64_t* bytes);

}  // namespace net::android::traffic_stats

#endif  // NET_ANDROID_TRAFFIC_STATS_H_
