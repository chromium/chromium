// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DEBUG_UTILS_H_
#define CONTENT_COMMON_DEBUG_UTILS_H_

namespace content {

// There are cases that require debugging of complex multiprocess scenarios in
// which tools such as DumpWithoutCrashing and tracing can be useful.
// The goal of this enum is to be able to log to UMA when such complex scenarios
// occur with two goals:
// * Get an idea of the magnitute of the problem, since just crash reports
//   cannot express this, given they are throttled client side.
// * For clients which have continuous tracing enabled, upload a snapshot of the
//   trace to aid understanding of the interactions between all processes.
//
// Usage: Add a new value to the DebugScenario enum below and call
//   CaptureTraceForNavigationDebugScenario(YOUR_NEW_ENUM_VALUE);
// from the location where you are hitting the scenario you care about.
//
// NOTE: Do not renumber elements in this list. Add new entries at the end.
// Items may be renamed but do not change the values. We rely on the enum values
// in histograms.

enum class DebugScenario {
  kDebugSameDocNavigationDocIdMismatch = 1,

  // After making changes, you MUST update the histograms xml by running:
  // "python tools/metrics/histograms/update_debug_scenarios.py"
  kMaxValue = kDebugSameDocNavigationDocIdMismatch
};

// The tracing categories enabled for debugging navigation scenarios can be
// found in server-side studies config for slow reports.
void CaptureTraceForNavigationDebugScenario(DebugScenario scenario);

}  // namespace content

#endif  // CONTENT_COMMON_DEBUG_UTILS_H_
