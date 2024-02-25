// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_METRICS_HISTOGRAM_SHARED_MEMORY_CONFIG_H_
#define CONTENT_BROWSER_METRICS_HISTOGRAM_SHARED_MEMORY_CONFIG_H_

#include <optional>

#include "base/metrics/histogram_shared_memory.h"
#include "content/common/content_export.h"

namespace content {

// Returns the histogram shared memory configuration for |process_type|, if any.
CONTENT_EXPORT std::optional<base::HistogramSharedMemory::Config>
GetHistogramSharedMemoryConfig(int process_type);

}  // namespace content

#endif  // CONTENT_BROWSER_METRICS_HISTOGRAM_SHARED_MEMORY_CONFIG_H_
