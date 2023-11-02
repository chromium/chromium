// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/cpu_time_metrics.h"

#include "content/common/android/cpu_time_metrics_internal.h"

namespace content {

void SetupCpuTimeMetrics() {
  internal::ProcessCpuTimeMetrics::GetInstance();
}

}  // namespace content
