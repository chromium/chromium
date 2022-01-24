// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_TEST_TEST_API_OBSERVER_H_
#define EXTENSIONS_BROWSER_API_TEST_TEST_API_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class TestSendMessageFunction;

// Allows browser-side test code to be notified of calls to chrome.test.*
// functions from a test extension.
class TestApiObserver : public base::CheckedObserver {
 public:
  // Called on chrome.test.notifyPass.
  virtual void OnTestPassed(content::BrowserContext* browser_context) {}

  // Called on chrome.test.notifyFail.
  virtual void OnTestFailed(content::BrowserContext* browser_context,
                            const std::string& message) {}

  // Called on chrome.test.sendMessage.
  // If the observer will reply to |function|, returns true.
  virtual bool OnTestMessage(TestSendMessageFunction* function,
                             const std::string& message);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_TEST_TEST_API_OBSERVER_H_
