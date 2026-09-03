// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_TEST_REALM_ROOT_H_
#define FUCHSIA_WEB_COMMON_TEST_TEST_REALM_ROOT_H_

#include <fuchsia/component/cpp/fidl.h>
#include <lib/sys/component/cpp/testing/realm_builder.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"

namespace test {

// Manages a `RealmRoot` built from a `RealmBuilder` and observes component
// lifecycle events within it via `fuchsia.component.EventStream` to detect
// abnormal component terminations and crashes.
//
// The constructor synchronously connects to `fuchsia.component.EventStream` and
// blocks until Component Manager acknowledges the subscription before invoking
// `realm_builder.Build()`. All lifecycle events within the constructed realm
// (matching `realm_prefix()`) are monitored.
//
// `Teardown()` tears down the realm and verifies that all components started
// within it have stopped cleanly (or with an expected abnormal termination).
class TestRealmRoot {
 public:
  explicit TestRealmRoot(::component_testing::RealmBuilder realm_builder);
  ~TestRealmRoot();

  TestRealmRoot(const TestRealmRoot&) = delete;
  TestRealmRoot& operator=(const TestRealmRoot&) = delete;

  TestRealmRoot(TestRealmRoot&&) = delete;
  TestRealmRoot& operator=(TestRealmRoot&&) = delete;

  // Tears down the realm root, waits for teardown to complete, and verifies
  // that all started components within the realm terminated cleanly.
  void Teardown();

  // Registers a component moniker (relative to this realm root, e.g.
  // "cast_runner") that is expected to terminate abnormally. The test will
  // fail if the specified component terminates normally or is never observed
  // to stop.
  void ExpectAbnormalTermination(std::string_view relative_moniker);

  // Provides access to the owned `RealmRoot`.
  ::component_testing::RealmRoot* operator->() { return &realm_root_.value(); }
  const ::component_testing::RealmRoot* operator->() const {
    return &realm_root_.value();
  }
  ::component_testing::RealmRoot& realm_root() { return realm_root_.value(); }
  const ::component_testing::RealmRoot& realm_root() const {
    return realm_root_.value();
  }

  // Returns the realm prefix (e.g. "realm_builder:auto-<id>/").
  const std::string& realm_prefix() const { return realm_prefix_; }

 private:
  void Connect();
  void ListenNext();
  void OnEvents(std::vector<fuchsia::component::Event> events);

  std::optional<::component_testing::RealmRoot> realm_root_;
  std::string realm_prefix_;

  fuchsia::component::EventStreamPtr event_stream_;
  base::flat_set<std::string> running_components_;
  base::flat_set<std::string> expected_abnormal_terminations_;
};

}  // namespace test

#endif  // FUCHSIA_WEB_COMMON_TEST_TEST_REALM_ROOT_H_
