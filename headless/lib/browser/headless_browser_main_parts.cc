// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_devtools.h"
#include "headless/lib/browser/headless_screen.h"

namespace headless {

HeadlessBrowserMainParts::HeadlessBrowserMainParts(
    const content::MainFunctionParams& parameters,
    HeadlessBrowserImpl* browser)
    : parameters_(parameters), browser_(browser) {}

HeadlessBrowserMainParts::~HeadlessBrowserMainParts() = default;

void HeadlessBrowserMainParts::PreMainMessageLoopRun() {
  if (browser_->options()->DevtoolsServerEnabled()) {
    StartLocalDevToolsHttpHandler(browser_->options());
    devtools_http_handler_started_ = true;
  }
  browser_->PlatformInitialize();
  browser_->RunOnStartCallback();

  if (parameters_.ui_task) {
    parameters_.ui_task->Run();
    delete parameters_.ui_task;
    run_message_loop_ = false;
  }
}

void HeadlessBrowserMainParts::PreDefaultMainMessageLoopRun(
    base::OnceClosure quit_closure) {
  quit_main_message_loop_ = std::move(quit_closure);
}

bool HeadlessBrowserMainParts::MainMessageLoopRun(int* result_code) {
  return !run_message_loop_;
}

void HeadlessBrowserMainParts::PostMainMessageLoopRun() {
  if (devtools_http_handler_started_) {
    StopLocalDevToolsHttpHandler();
    devtools_http_handler_started_ = false;
  }
}

void HeadlessBrowserMainParts::QuitMainMessageLoop() {
  if (quit_main_message_loop_)
    std::move(quit_main_message_loop_).Run();
}

}  // namespace headless
