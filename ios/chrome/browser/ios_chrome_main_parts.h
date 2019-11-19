// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IOS_CHROME_MAIN_PARTS_H_
#define IOS_CHROME_BROWSER_IOS_CHROME_MAIN_PARTS_H_

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "ios/chrome/browser/ios_chrome_field_trials.h"
#include "ios/web/public/init/web_main_parts.h"

class ApplicationContextImpl;
class PrefService;

class IOSChromeMainParts : public web::WebMainParts {
 public:
  explicit IOSChromeMainParts(const base::CommandLine& parsed_command_line);
  ~IOSChromeMainParts() override;

 private:
  // web::WebMainParts implementation.
  void PreMainMessageLoopStart() override;
  void PreCreateThreads() override;
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  // Sets up the field trials and related initialization. Call only after
  // about:flags have been converted to switches.
  void SetupFieldTrials();

  // Constructs the metrics service and initializes metrics recording.
  void SetupMetrics();

  // Starts recording of metrics. This can only be called after we have a file
  // thread.
  void StartMetricsRecording();

  const base::CommandLine& parsed_command_line_;

  std::unique_ptr<ApplicationContextImpl> application_context_;

  // Statistical testing infrastructure for the entire browser. NULL until
  // SetUpMetricsAndFieldTrials is called.
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  PrefService* local_state_;

  IOSChromeFieldTrials ios_field_trials_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeMainParts);
};

#endif  // IOS_CHROME_BROWSER_IOS_CHROME_MAIN_PARTS_H_
