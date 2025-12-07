// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/result_catcher.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

ResultCatcher::ResultCatcher() {
  test_api_observation_.Observe(TestApiObserverRegistry::GetInstance());
}

ResultCatcher::~ResultCatcher() = default;

bool ResultCatcher::GetNextResult() {
  // Depending on the tests, multiple results can come in from a single call to
  // RunLoop::Run() so we maintain a queue of results and just pull them off as
  // the test calls this, going to the run loop only when the queue is empty.
  if (results_.empty()) {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = content::GetDeferredQuitTaskForRunLoop(&run_loop);
    run_loop.Run();
  }

  // Can happen if the test timed out and never produced a result.
  if (results_.empty()) {
    ADD_FAILURE() << "ResultCatcher never received a result.";
    return false;
  }

  bool ret = results_.front();
  results_.pop_front();
  message_ = messages_.front();
  messages_.pop_front();
  return ret;
}

void ResultCatcher::OnTestPassed(content::BrowserContext* browser_context) {
  if (IsRelevantBrowserContext(browser_context)) {
    return;
  }

  VLOG(1) << "Got chrome.test.notifyPass notification.";
  results_.push_back(true);
  messages_.push_back(std::string());
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

void ResultCatcher::OnTestFailed(content::BrowserContext* browser_context,
                                 const std::string& message) {
  if (IsRelevantBrowserContext(browser_context)) {
    return;
  }

  VLOG(1) << "Got chrome.test.notifyFail notification.";
  results_.push_back(false);
  messages_.push_back(message);
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

bool ResultCatcher::IsRelevantBrowserContext(
    content::BrowserContext* browser_context) const {
  return browser_context_restriction_ &&
         browser_context != browser_context_restriction_;
}

}  // namespace extensions
