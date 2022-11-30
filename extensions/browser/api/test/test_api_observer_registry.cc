// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/test/test_api_observer_registry.h"

#include <ostream>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace extensions {

TestApiObserverRegistry::TestApiObserverRegistry() = default;
TestApiObserverRegistry::~TestApiObserverRegistry() = default;

// static
TestApiObserverRegistry* TestApiObserverRegistry::GetInstance() {
  static base::NoDestructor<TestApiObserverRegistry> registry;
  return registry.get();
}

void TestApiObserverRegistry::NotifyTestPassed(
    content::BrowserContext* browser_context) {
  for (auto& observer : observers_) {
    observer.OnTestPassed(browser_context);
  }
}

void TestApiObserverRegistry::NotifyTestFailed(
    content::BrowserContext* browser_context,
    const std::string& message) {
  for (auto& observer : observers_) {
    observer.OnTestFailed(browser_context, message);
  }
}

bool TestApiObserverRegistry::NotifyTestMessage(
    TestSendMessageFunction* function,
    const std::string& message) {
  bool any_listener_will_respond = false;
  for (auto& observer : observers_) {
    bool listener_will_respond = observer.OnTestMessage(function, message);
    DCHECK(!any_listener_will_respond || !listener_will_respond)
        << "Only one listener may reply.";
    any_listener_will_respond =
        any_listener_will_respond || listener_will_respond;
  }
  return any_listener_will_respond;
}

void TestApiObserverRegistry::NotifyScriptResult(
    const base::Value& result_value) {
  for (auto& observer : observers_)
    observer.OnScriptResult(result_value);
}

void TestApiObserverRegistry::AddObserver(TestApiObserver* observer) {
  observers_.AddObserver(observer);
}

void TestApiObserverRegistry::RemoveObserver(TestApiObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace extensions
