// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRACING_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_TRACING_TEST_UTILS_H_

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "ios/chrome/browser/tracing/ios_tracing_controller.h"
#include "services/tracing/public/cpp/trace_startup_config.h"

// An RAII test helper for IOSTracingController that initializes Perfetto and
// background tracing during its lifetime, and cleans up the global state on
// destruction.
class IOSTracingControllerForTesting : public IOSTracingController {
 public:
  explicit IOSTracingControllerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr);
  explicit IOSTracingControllerForTesting(
      const base::CommandLine& command_line,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr);
  ~IOSTracingControllerForTesting() override;

 private:
  void Init(scoped_refptr<base::SequencedTaskRunner> task_runner);

  tracing::TraceStartupConfig startup_config_;
};

#endif  // IOS_CHROME_BROWSER_TRACING_TEST_UTILS_H_
