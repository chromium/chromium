// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/files/file_path.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"
#include "headless/public/headless_browser.h"

namespace headless {

class HeadlessBrowserImpl;

class HeadlessBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit HeadlessBrowserMainParts(
      const content::MainFunctionParams& parameters,
      HeadlessBrowserImpl* browser);
  ~HeadlessBrowserMainParts() override;

  // content::BrowserMainParts implementation:
  void PreMainMessageLoopRun() override;
  void PreDefaultMainMessageLoopRun(base::OnceClosure quit_closure) override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostMainMessageLoopRun() override;
#if defined(OS_MACOSX)
  void PreMainMessageLoopStart() override;
#endif

  void QuitMainMessageLoop();

 private:
  const content::MainFunctionParams parameters_;  // For running browser tests.
  HeadlessBrowserImpl* browser_;  // Not owned.

  bool run_message_loop_ = true;
  bool devtools_http_handler_started_ = false;
  base::OnceClosure quit_main_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserMainParts);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_MAIN_PARTS_H_
