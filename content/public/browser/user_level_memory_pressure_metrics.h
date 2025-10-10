// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_USER_LEVEL_MEMORY_PRESSURE_METRICS_H_
#define CONTENT_PUBLIC_BROWSER_USER_LEVEL_MEMORY_PRESSURE_METRICS_H_

#include <optional>

#include "base/byte_count.h"
#include "content/common/content_export.h"

namespace content {

// A struct to hold various memory metrics for experimental purposes.
struct UserLevelMemoryPressureMetrics {
  base::ByteCount total_private_footprint;
  base::ByteCount available_memory;
  int total_process_count;
  int visible_renderer_count;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_USER_LEVEL_MEMORY_PRESSURE_METRICS_H_
