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
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_manager.h"
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
class IOSTracingController : public tracing::BackgroundTracingManager {
 public:
  static IOSTracingController& GetInstance();
  static bool HasInstance();

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
  void SetLatestIncognitoLaunchedForTesting(base::TimeTicks timestamp);

  tracing::StartupTracingController* startup_tracing_controller() {
    return startup_tracing_controller_.get();
  }

 protected:
  // tracing::BackgroundTracingManager overrides:
  bool GetBackgroundStartupTracingEnabled() const override;
  bool IsRecordingAllowed(bool privacy_filter_enabled,
                          base::TimeTicks scenario_start_time) override;
  bool ShouldSaveUnuploadedTrace() override;
  std::string RecordSerializedSystemProfileMetrics() override;
  std::optional<base::FilePath> GetLocalTracesDirectory() override;
  void MaybeConstructPendingAgents() override;

 private:
  friend class base::NoDestructor<IOSTracingController>;
  friend class IOSTracingControllerTest;
  IOSTracingController();
  ~IOSTracingController() override;

  void Initialize();
  void OnIncognitoSessionStateChanged(bool has_incognito_tabs);

  std::unique_ptr<tracing::StartupTracingController>
      startup_tracing_controller_;

  std::unique_ptr<base::tracing::PerfettoPlatform> platform_;

  base::TimeTicks latest_incognito_launched_;
  base::CallbackListSubscription incognito_tracker_subscription_;

  base::WeakPtrFactory<IOSTracingController> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_TRACING_IOS_TRACING_CONTROLLER_H_
