// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include <memory.h>
#include <stdio.h>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/headless/clipboard/headless_clipboard.h"
#include "components/headless/select_file_dialog/headless_select_file_dialog.h"
#include "content/public/common/result_codes.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_devtools.h"
#include "headless/lib/browser/headless_screen.h"

namespace headless {

HeadlessBrowserMainParts::HeadlessBrowserMainParts(HeadlessBrowserImpl& browser)
    : browser_(browser) {}

HeadlessBrowserMainParts::~HeadlessBrowserMainParts() = default;

int HeadlessBrowserMainParts::PreMainMessageLoopRun() {
  SetHeadlessClipboardForCurrentThread();
  browser_->PreMainMessageLoopRun();
  MaybeStartLocalDevToolsHttpHandler();
  HeadlessSelectFileDialogFactory::SetUp();
  return content::RESULT_CODE_NORMAL_EXIT;
}

void HeadlessBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  browser_->WillRunMainMessageLoop(*run_loop);
}

void HeadlessBrowserMainParts::PostMainMessageLoopRun() {
  // HeadlessBrowserImpl::Shutdown() is supposed to remove all browser contexts
  // and therefore all associated web contents, however crbug.com/1342152
  // implies it may not be happening.
  CHECK_EQ(0U, browser_->GetAllBrowserContexts().size());
  if (devtools_http_handler_started_) {
    StopLocalDevToolsHttpHandler();
    devtools_http_handler_started_ = false;
  }
  browser_->PostMainMessageLoopRun();
}

void HeadlessBrowserMainParts::MaybeStartLocalDevToolsHttpHandler() {
  if (!browser_->ShouldStartDevToolsServer()) {
    return;
  }

  StartLocalDevToolsHttpHandler(&*browser_);
  devtools_http_handler_started_ = true;
}

}  // namespace headless
