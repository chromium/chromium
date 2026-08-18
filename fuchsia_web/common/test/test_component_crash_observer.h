// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_TEST_COMPONENT_CRASH_OBSERVER_H_
#define FUCHSIA_WEB_COMMON_TEST_TEST_COMPONENT_CRASH_OBSERVER_H_

#include <fuchsia/component/cpp/fidl.h>

#include <vector>

namespace test {

// Observes component lifecycle events via the `fuchsia.component.EventStream`
// protocol to detect abnormal component terminations and crashes.
class TestComponentCrashObserver {
 public:
  TestComponentCrashObserver();
  ~TestComponentCrashObserver();

  TestComponentCrashObserver(const TestComponentCrashObserver&) = delete;
  TestComponentCrashObserver& operator=(const TestComponentCrashObserver&) =
      delete;

  // Drains pending lifecycle events and verifies that all observed component
  // stops were clean.
  void VerifyNoCrashes();

 private:
  void Connect();
  void ListenNext();
  void OnEvents(std::vector<fuchsia::component::Event> events);

  fuchsia::component::EventStreamPtr event_stream_;
};

}  // namespace test

#endif  // FUCHSIA_WEB_COMMON_TEST_TEST_COMPONENT_CRASH_OBSERVER_H_
