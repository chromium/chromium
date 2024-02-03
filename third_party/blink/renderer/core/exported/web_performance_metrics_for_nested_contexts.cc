// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_performance_metrics_for_nested_contexts.h"

#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

void WebPerformanceMetricsForNestedContexts::Reset() {
  private_.Reset();
}

void WebPerformanceMetricsForNestedContexts::Assign(
    const WebPerformanceMetricsForNestedContexts& other) {
  private_ = other.private_;
}

WebPerformanceMetricsForNestedContexts::WebPerformanceMetricsForNestedContexts(
    WindowPerformance* performance)
    : private_(performance) {}

WebPerformanceMetricsForNestedContexts&
WebPerformanceMetricsForNestedContexts::operator=(
    WindowPerformance* performance) {
  private_ = performance;
  return *this;
}

std::optional<base::TimeTicks>
WebPerformanceMetricsForNestedContexts::UnloadStart() const {
  return private_->timingForReporting()->UnloadStart();
}

std::optional<base::TimeTicks>
WebPerformanceMetricsForNestedContexts::UnloadEnd() const {
  return private_->timingForReporting()->UnloadEnd();
}

std::optional<base::TimeTicks>
WebPerformanceMetricsForNestedContexts::CommitNavigationEnd() const {
  return private_->timingForReporting()->CommitNavigationEnd();
}
}  // namespace blink
