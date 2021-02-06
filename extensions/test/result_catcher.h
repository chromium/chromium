// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_RESULT_CATCHER_H_
#define EXTENSIONS_TEST_RESULT_CATCHER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Helper class that observes tests failing or passing. Observation starts
// when the class is constructed. Get the next result by calling
// GetNextResult() and message() if GetNextResult() return false. If there
// are no results, this method will pump the UI message loop until one is
// received.
class ResultCatcher : public content::NotificationObserver {
 public:
  ResultCatcher();
  ~ResultCatcher() override;

  // Pumps the UI loop until a notification is received that an API test
  // succeeded or failed. Returns true if the test succeeded, false otherwise.
  bool GetNextResult() WARN_UNUSED_RESULT;

  void RestrictToBrowserContext(content::BrowserContext* context) {
    browser_context_restriction_ = context;
  }

  const std::string& message() { return message_; }

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar registrar_;

  // A sequential list of pass/fail notifications from the test extension(s).
  base::circular_deque<bool> results_;

  // If it failed, what was the error message?
  base::circular_deque<std::string> messages_;
  std::string message_;

  // If non-NULL, we will listen to events from this BrowserContext only.
  content::BrowserContext* browser_context_restriction_;

  // Only set if we're in a nested run loop waiting for results from
  // the extension.
  base::OnceClosure quit_closure_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_RESULT_CATCHER_H_
