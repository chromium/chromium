// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_STARTUP_TRACING_CONTROLLER_H_
#define CONTENT_BROWSER_TRACING_STARTUP_TRACING_CONTROLLER_H_

#include "base/threading/sequence_bound.h"

namespace content {

// Class responsible for starting and stopping startup tracing as configured by
// StartupTracingConfig. All interactions with it are limited to UI thread, but
// the actual logic lives on a background ThreadPool sequence.
class StartupTracingController {
 public:
  StartupTracingController();
  ~StartupTracingController();

  static StartupTracingController& GetInstance();

  void StartIfNeeded();
  void WaitUntilStopped();

 private:
  void Stop(base::OnceClosure on_finished_callback);

  void OnStoppedOnUIThread();

  enum class State {
    kRunning,
    kNotRunning,
  };
  State state_ = State::kNotRunning;

  // All actual interactions with the tracing service and the process of writing
  // files happens on a background thread.
  class BackgroundTracer;
  base::SequenceBound<BackgroundTracer> background_tracer_;

  base::OnceClosure on_tracing_finished_;
  base::FilePath output_file_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_STARTUP_TRACING_CONTROLLER_H_
