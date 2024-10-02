// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_TEST_NAVIGATION_LISTENER_H_
#define FUCHSIA_WEB_COMMON_TEST_TEST_NAVIGATION_LISTENER_H_

#include <fuchsia/web/cpp/fidl.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"

class GURL;

// Observes navigation events and enables test code to block until a desired
// navigational state is observed.
// When run with -v1, information about navigation state transitions and
// unmet test expectations are logged.
class TestNavigationListener final
    : public fuchsia::web::NavigationEventListener {
 public:
  using BeforeAckCallback =
      base::RepeatingCallback<void(const fuchsia::web::NavigationState& change,
                                   OnNavigationStateChangedCallback)>;

  TestNavigationListener();
  ~TestNavigationListener() override;

  TestNavigationListener(const TestNavigationListener&) = delete;
  TestNavigationListener& operator=(const TestNavigationListener&) = delete;

  // Spins a RunLoop until the navigation state of the page matches the fields
  // of |expected_state| that have been set.
  void RunUntilNavigationStateMatches(
      const fuchsia::web::NavigationState& expected_state);

  // Calls RunUntilNavigationStateMatches with a NavigationState that has
  // the main document loaded and the normal page type.
  void RunUntilLoaded();

  // Calls RunUntilNavigationStateMatches with a NavigationState that has
  // |expected_url|, the normal page type, and the main document loaded.
  void RunUntilUrlEquals(const GURL& expected_url);

  // Calls RunUntilNavigationStateMatches with a NavigationState that has
  // |expected_title| and the normal page type.
  void RunUntilTitleEquals(std::string_view expected_title);

  // Calls RunUntilNavigationStateMatches with a NavigationState that has
  // |expected_url|, |expected_title|, and the normal page type.
  void RunUntilUrlAndTitleEquals(const GURL& expected_url,
                                 std::string_view expected_title);

  // Calls RunUntilNavigationStateMatches with a NavigationState that has
  // the error page type, |expected_title|, and the main document loaded.
  void RunUntilErrorPageIsLoadedAndTitleEquals(std::string_view expected_title);

  // Calls RunUntilNavigationStateMatches with a NavigationState that has
  // all the expected fields and the normal page type.
  void RunUntilUrlTitleBackForwardEquals(const GURL& expected_url,
                                         std::string_view expected_title,
                                         bool expected_can_go_back,
                                         bool expected_can_go_forward);

  // Returns the title.
  std::string title() { return current_state_.title(); }

  // Returns the current navigation state.
  fuchsia::web::NavigationState* current_state() { return &current_state_; }

  // Returns the last received changes.
  fuchsia::web::NavigationState* last_changes() { return &last_changes_; }

  // Register a callback which intercepts the execution of the event
  // acknowledgement callback. |before_ack| takes ownership of the
  // acknowledgement callback and the responsibility for executing it.
  // The default behavior can be restored by providing an unbound callback for
  // |before_ack|.
  void SetBeforeAckHook(BeforeAckCallback before_ack);

 private:
  struct FailureReason {
    const char* field_name;
    std::string expected;
  };

  // fuchsia::web::NavigationEventListener implementation.
  void OnNavigationStateChanged(
      fuchsia::web::NavigationState change,
      OnNavigationStateChangedCallback callback) override;

  // Compare the current state with all fields of |expected_state_| that have
  // been set. Any expectation mismatches will be recorded in |failure_reasons|,
  // if set.
  bool AllFieldsMatch(std::vector<FailureReason>* failure_reasons);

  void QuitLoopIfAllFieldsMatch(
      base::RepeatingClosure quit_run_loop_closure,
      BeforeAckCallback before_ack_callback,
      const fuchsia::web::NavigationState& change,
      fuchsia::web::NavigationEventListener::OnNavigationStateChangedCallback
          ack_callback);

  fuchsia::web::NavigationState current_state_;
  fuchsia::web::NavigationState last_changes_;

  // Set for the duration of a call to RunUntilNavigationStateMatches().
  const fuchsia::web::NavigationState* expected_state_ = nullptr;

  BeforeAckCallback before_ack_;
};

#endif  // FUCHSIA_WEB_COMMON_TEST_TEST_NAVIGATION_LISTENER_H_
