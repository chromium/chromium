// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains definitions for media caching metrics.

#ifndef NET_BASE_CACHE_METRICS_H_
#define NET_BASE_CACHE_METRICS_H_

#include "base/metrics/histogram_macros.h"
#include "net/base/net_export.h"

namespace net {

// UMA histogram enumerations for indicating whether media caching
// is enabled or disabled.
enum class MediaResponseCacheType {
  kMediaResponseTransactionCacheDisabled = 0,
  kMediaResponseTransactionCacheEnabled = 1,
  kMaxValue = kMediaResponseTransactionCacheEnabled
};

NET_EXPORT void MediaCacheStatusResponseHistogram(MediaResponseCacheType type);

}  // namespace net

#endif  // NET_BASE_CACHE_METRICS_H_
