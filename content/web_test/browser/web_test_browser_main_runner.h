// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_MAIN_RUNNER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_MAIN_RUNNER_H_

#include "base/files/scoped_temp_dir.h"
#include "content/public/common/main_function_params.h"

namespace content {

class WebTestBrowserMainRunner {
 public:
  // Run at the beginning of startup, before any child processes (including the
  // renderer zygote) are launched. Any command line flags should be set in
  // here.
  void Initialize();

  // Main routine for running as the Browser process.
  void RunBrowserMain(content::MainFunctionParams parameters);

 private:
  base::ScopedTempDir browser_context_path_for_web_tests_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_MAIN_RUNNER_H_
