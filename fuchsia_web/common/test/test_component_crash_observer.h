// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_TEST_COMPONENT_CRASH_OBSERVER_H_
#define FUCHSIA_WEB_COMMON_TEST_TEST_COMPONENT_CRASH_OBSERVER_H_

#include <fuchsia/component/cpp/fidl.h>

#include <string>
#include <vector>

#include "base/containers/flat_set.h"

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

  // Registers a dynamic component moniker that is expected to terminate
  // abnormally. The test will fail if the specified component is observed
  // to stop normally, or is never observed to stop.
  void ExpectAbnormalTermination(std::string moniker);

  // Drains pending lifecycle events and verifies that all observed dynamic
  // component stops were clean, and that all expected abnormal terminations
  // occurred.
  void VerifyNoCrashes();

 private:
  void Connect();
  void ListenNext();
  void OnEvents(std::vector<fuchsia::component::Event> events);

  fuchsia::component::EventStreamPtr event_stream_;
  base::flat_set<std::string> running_components_;
  base::flat_set<std::string> expected_abnormal_terminations_;
};

}  // namespace test

#endif  // FUCHSIA_WEB_COMMON_TEST_TEST_COMPONENT_CRASH_OBSERVER_H_
