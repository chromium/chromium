// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_MAIN_RUNNER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_MAIN_RUNNER_H_

#include <memory>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/main_function_params.h"

namespace content {

struct MainFunctionParams;

// This class is responsible for browser initialization, running and shutdown.
class CONTENT_EXPORT BrowserMainRunner {
 public:
  virtual ~BrowserMainRunner() {}

  // Create a new BrowserMainRunner object.
  static std::unique_ptr<BrowserMainRunner> Create();

  // Returns true if the BrowserMainRunner has exited the main loop.
  static bool ExitedMainMessageLoop();

  // Overrides the final process exit code for a normal browser exit.
  //
  // Can be called by the embedder at any point before or during shutdown (e.g.
  // during `ChromeBrowserMainParts::PostDestroyThreads` or
  // `ShutdownPostThreadsStop`) to request that a custom exit code (such as a
  // relaunch signal code) be returned instead of
  // `content::RESULT_CODE_NORMAL_EXIT`.
  //
  // This override is only applied if the browser process ran and completed
  // normally (i.e. `Run()` returned `RESULT_CODE_NORMAL_EXIT`). If early
  // initialization failed or an abnormal/error exit code was returned, this
  // override is ignored and the original failure exit code is returned.
  //
  // `code` must be an embedder result code (>= RESULT_CODE_LAST_CODE).
  static void SetOverrideResultCode(int code);

  // Initialize all necessary browser state. Returning a non-negative value
  // indicates that initialization failed, and the returned value is used as
  // the exit code for the process.
  virtual int Initialize(content::MainFunctionParams parameters) = 0;

  // Perform the default run logic.
  virtual int Run() = 0;

  // Shut down the browser state.
  virtual void Shutdown() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_MAIN_RUNNER_H_
