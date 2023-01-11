// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_RESULT_CATCHER_H_
#define EXTENSIONS_TEST_RESULT_CATCHER_H_

#include <string>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/api/test/test_api_observer.h"
#include "extensions/browser/api/test/test_api_observer_registry.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Helper class that observes tests failing or passing. Observation starts
// when the class is constructed. Get the next result by calling
// GetNextResult() and message() if GetNextResult() return false. If there
// are no results, this method will pump the UI message loop until one is
// received.
class ResultCatcher : public TestApiObserver {
 public:
  ResultCatcher();
  ~ResultCatcher() override;

  // Pumps the UI loop until a notification is received that an API test
  // succeeded or failed. Returns true if the test succeeded, false otherwise.
  [[nodiscard]] bool GetNextResult();

  void RestrictToBrowserContext(content::BrowserContext* context) {
    browser_context_restriction_ = context;
  }

  const std::string& message() { return message_; }

 private:
  // TestApiObserver:
  void OnTestPassed(content::BrowserContext* browser_context) override;
  void OnTestFailed(content::BrowserContext* browser_context,
                    const std::string& message) override;

  // A sequential list of pass/fail notifications from the test extension(s).
  base::circular_deque<bool> results_;

  // If it failed, what was the error message?
  base::circular_deque<std::string> messages_;
  std::string message_;

  // If non-NULL, we will listen to events from this BrowserContext only.
  raw_ptr<content::BrowserContext> browser_context_restriction_;

  // Only set if we're in a nested run loop waiting for results from
  // the extension.
  base::OnceClosure quit_closure_;

  base::ScopedObservation<TestApiObserverRegistry, TestApiObserver>
      test_api_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_RESULT_CATCHER_H_
