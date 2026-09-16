// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRACING_IOS_TRACING_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TRACING_IOS_TRACING_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>
#import <string>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_manager.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace base {
class SequencedTaskRunner;
}

namespace tracing {
class StartupTracingController;
}

// A lightweight iOS-specific tracing manager for developer profiling.
//
// Initializes Perfetto with the kInProcessBackend and delegates to the
// shared tracing::StartupTracingController to handle --trace-startup flags.
class IOSTracingController : public tracing::BackgroundTracingManager {
 public:
  static IOSTracingController& GetInstance();
  static bool HasInstance();

  static void CreateInstance();

  IOSTracingController(const IOSTracingController&) = delete;
  IOSTracingController& operator=(const IOSTracingController&) = delete;

  // Creates a standard Perfetto TraceConfig tailored for local developer
  // debugging (e.g., Flamegraphs, System Metrics, standard Track Events)
  // using a large 50MB in-memory buffer.
  perfetto::TraceConfig CreateDeveloperTraceConfig();

  void SetLatestIncognitoLaunchedForTesting(base::TimeTicks timestamp);

  tracing::StartupTracingController* startup_tracing_controller() {
    return startup_tracing_controller_.get();
  }

 protected:
  friend class base::NoDestructor<IOSTracingController>;
  friend class IOSTracingControllerTest;
  friend class IOSTracingControllerForTesting;

  explicit IOSTracingController(
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr);
  ~IOSTracingController() override;

  void Initialize();

  // tracing::BackgroundTracingManager overrides:
  bool GetBackgroundStartupTracingEnabled() const override;
  bool IsRecordingAllowed(bool is_local_scenario,
                          base::TimeTicks scenario_start_time) override;
  bool ShouldSaveUnuploadedTrace() override;
  std::string RecordSerializedSystemProfileMetrics() override;
  std::optional<base::FilePath> GetLocalTracesDirectory() override;
  void MaybeConstructPendingAgents() override;

 private:
  void OnIncognitoSessionStateChanged(bool has_incognito_tabs);

  std::unique_ptr<tracing::StartupTracingController>
      startup_tracing_controller_;

  base::TimeTicks latest_incognito_launched_;
  base::CallbackListSubscription incognito_tracker_subscription_;

  base::WeakPtrFactory<IOSTracingController> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_TRACING_IOS_TRACING_CONTROLLER_H_
