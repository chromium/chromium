// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_MEMORY_PRESSURE_EVALUATOR_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_MEMORY_PRESSURE_EVALUATOR_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "base/util/memory_pressure/system_memory_pressure_evaluator.h"

namespace util {
class MemoryPressureVoter;
}

// Synthesizes MemoryPressureLevel values & notifications by comparing the total
// memory usage of the web Context processes against a target total.
class WebEngineMemoryPressureEvaluator
    : public util::SystemMemoryPressureEvaluator {
 public:
  explicit WebEngineMemoryPressureEvaluator(
      std::unique_ptr<util::MemoryPressureVoter> voter);

  ~WebEngineMemoryPressureEvaluator() override;

  WebEngineMemoryPressureEvaluator(const WebEngineMemoryPressureEvaluator&) =
      delete;
  WebEngineMemoryPressureEvaluator& operator=(
      const WebEngineMemoryPressureEvaluator&) = delete;

 private:
  void CheckMemoryPressure();

  // Periodic timer used to trigger sampling of memory usage.
  base::RepeatingTimer timer_;

  // Next time at which to notify moderate memory pressure.
  base::TimeTicks next_moderate_notify_time_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_MEMORY_PRESSURE_EVALUATOR_H_
