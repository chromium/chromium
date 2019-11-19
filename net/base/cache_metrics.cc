// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cache_metrics.h"

namespace net {

void MediaCacheStatusResponseHistogram(MediaResponseCacheType type) {
  UMA_HISTOGRAM_ENUMERATION("Net.MediaCache.Response.EnabledOrDisabled", type);
}

}  // namespace net
