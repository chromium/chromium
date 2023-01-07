// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_TEST_TEST_API_OBSERVER_REGISTRY_H_
#define EXTENSIONS_BROWSER_API_TEST_TEST_API_OBSERVER_REGISTRY_H_

#include <string>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "extensions/browser/api/test/test_api_observer.h"

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class TestSendMessageFunction;

class TestApiObserverRegistry {
 public:
  TestApiObserverRegistry(const TestApiObserverRegistry&) = delete;
  TestApiObserverRegistry& operator=(const TestApiObserverRegistry&) = delete;

  // Unfortunately, most existing observers do not specify a BrowserContext for
  // which they intend to listen for events, so we resort to having a global
  // registry. Observers in tests where multiple BrowserContexts exist should
  // check which BrowserContext is associated with an event.
  static TestApiObserverRegistry* GetInstance();

  void NotifyTestPassed(content::BrowserContext* browser_context);
  void NotifyTestFailed(content::BrowserContext* browser_context,
                        const std::string& message);

  // Returns true if one of the TestApiObservers has indicated that it will
  // respond to the message.
  bool NotifyTestMessage(TestSendMessageFunction* function,
                         const std::string& message);

  // Notifies observers of a result sent via sendScriptResult.
  void NotifyScriptResult(const base::Value& result_value);

  void AddObserver(TestApiObserver* observer);
  void RemoveObserver(TestApiObserver* observer);

 private:
  friend class base::NoDestructor<TestApiObserverRegistry>;

  TestApiObserverRegistry();
  ~TestApiObserverRegistry();

  base::ObserverList<TestApiObserver> observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_TEST_TEST_API_OBSERVER_REGISTRY_H_
