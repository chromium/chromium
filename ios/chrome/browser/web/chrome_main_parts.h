// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_CHROME_MAIN_PARTS_H_
#define IOS_CHROME_BROWSER_WEB_CHROME_MAIN_PARTS_H_

#include <memory>

#include "base/allocator/buildflags.h"
#include "base/command_line.h"
#import "ios/chrome/browser/flags/ios_chrome_field_trials.h"
#include "ios/web/public/init/web_main_parts.h"

namespace heap_profiling {
class HeapProfilerController;
}

class ApplicationContextImpl;
class PrefService;
class IOSThreadProfiler;

class IOSChromeMainParts : public web::WebMainParts {
 public:
  explicit IOSChromeMainParts(const base::CommandLine& parsed_command_line);

  IOSChromeMainParts(const IOSChromeMainParts&) = delete;
  IOSChromeMainParts& operator=(const IOSChromeMainParts&) = delete;

  ~IOSChromeMainParts() override;

 private:
  // web::WebMainParts implementation.
  void PreEarlyInitialization() override;
  void PreCreateMainMessageLoop() override;
  void PreCreateThreads() override;
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  // Sets up the field trials and related initialization. Call only after
  // about:flags have been converted to switches. However,
  // `command_line_variation_ids` should be the value of the
  // "--force-variation-ids" switch before it is mutated. See
  // VariationsFieldTrialCreator::SetUpFieldTrials() for the format of
  // `command_line_variation_ids`.
  void SetUpFieldTrials(const std::string& command_line_variation_ids);

  // Constructs the metrics service and initializes metrics recording.
  void SetupMetrics();

  // Starts recording of metrics. This can only be called after we have a file
  // thread.
  void StartMetricsRecording();

  const base::CommandLine& parsed_command_line_;

  std::unique_ptr<ApplicationContextImpl> application_context_;

  PrefService* local_state_;

  IOSChromeFieldTrials ios_field_trials_;

  // A profiler that periodically samples stack traces. Used to understand
  // thread and process startup and normal behavior.
  std::unique_ptr<IOSThreadProfiler> sampling_profiler_;

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  // Manages heap (memory) profiling. Requires the allocator shim to be enabled.
  std::unique_ptr<heap_profiling::HeapProfilerController>
      heap_profiler_controller_;
#endif
};

#endif  // IOS_CHROME_BROWSER_WEB_CHROME_MAIN_PARTS_H_
