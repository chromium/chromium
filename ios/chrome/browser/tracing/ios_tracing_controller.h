// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRACING_IOS_TRACING_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TRACING_IOS_TRACING_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace base::tracing {
class PerfettoPlatform;
}

namespace tracing {
class StartupTracingController;
}

// A lightweight iOS-specific tracing manager for developer profiling.
//
// Initializes Perfetto with the kInProcessBackend and delegates to the
// shared tracing::StartupTracingController to handle --trace-startup flags.
class IOSTracingController {
 public:
  static IOSTracingController& GetInstance();

  static void CreateInstance();
  static void MaybeCreateInstanceForTesting();

  IOSTracingController(const IOSTracingController&) = delete;
  IOSTracingController& operator=(const IOSTracingController&) = delete;

  // Creates a standard Perfetto TraceConfig tailored for local developer
  // debugging (e.g., Flamegraphs, System Metrics, standard Track Events)
  // using a large 50MB in-memory buffer.
  perfetto::TraceConfig CreateDeveloperTraceConfig();

  // Resets the controller and Perfetto state. For testing only.
  void ResetForTesting();
  void InitializeForTesting();

  tracing::StartupTracingController* startup_tracing_controller() {
    return startup_tracing_controller_.get();
  }

 private:
  friend class base::NoDestructor<IOSTracingController>;
  IOSTracingController();
  ~IOSTracingController();

  void Initialize();

  std::unique_ptr<tracing::StartupTracingController>
      startup_tracing_controller_;

  std::unique_ptr<base::tracing::PerfettoPlatform> platform_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IOSTracingController> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_TRACING_IOS_TRACING_CONTROLLER_H_
