// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MEMORY_STATISTICS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MEMORY_STATISTICS_H_

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

struct BLINK_EXPORT WebMemoryStatistics {
  size_t partition_alloc_total_allocated_bytes;
  size_t blink_gc_total_allocated_bytes;

  WebMemoryStatistics()
      : partition_alloc_total_allocated_bytes(0),
        blink_gc_total_allocated_bytes(0) {}

  static WebMemoryStatistics Get();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MEMORY_STATISTICS_H_
