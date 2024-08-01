// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "content/public/browser/browser_thread.h"

namespace content {

class LivesOnUIThread {
 public:
  void BuggyCounterAccess();
  void BuggyIncrementCall();

  void Increment() VALID_BROWSER_THREAD_REQUIRED(BrowserThread::UI) {
    ++counter_;
  }

 private:
  int counter_ GUARDED_BY_BROWSER_THREAD(BrowserThread::UI);
};

void LivesOnUIThread::BuggyCounterAccess() {
  // Member access without (DCHECK|CHECK)_CURRENTLY_ON() assertion.
  ++counter_;  // expected-error {{writing variable 'counter_' requires holding BrowserThread checker 'GetBrowserThreadChecker(UI)' exclusively}}
}

void LivesOnUIThread::BuggyIncrementCall() {
  // Function call without (DCHECK|CHECK)_CURRENTLY_ON() assertion.
  Increment();  // expected-error {{calling function 'Increment' requires holding BrowserThread checker 'GetBrowserThreadChecker(UI)' exclusively}}
}

}  // namespace content
