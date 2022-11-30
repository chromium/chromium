// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/swap_metrics_driver.h"

#include <memory>

#include "base/time/time.h"

namespace content {

// static
std::unique_ptr<SwapMetricsDriver> SwapMetricsDriver::Create(
    std::unique_ptr<Delegate> delegate,
    const base::TimeDelta update_interval) {
  // SwapMetricsDriver isn't available on Windows for now.
  // TODO(bashi): Figure out a way to measure swap rates on Windows.
  return nullptr;
}

}  // namespace content
