// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_TEST_TEST_NAVIGATION_LISTENER_H_
#define FUCHSIA_BASE_TEST_TEST_NAVIGATION_LISTENER_H_

#include <fuchsia/web/cpp/fidl.h>
#include <string>

#include "base/callback.h"
#include "url/gurl.h"

namespace cr_fuchsia {

// Observes navigation events and enables test code to block until a desired
// navigational state is observed.
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
  void RunUntilTitleEquals(const base::StringPiece expected_title);

  // Calls RunUntilNavigationStateMatches with a NavigationState that has
  // |expected_url|, |expected_title|, and the normal page type.
  void RunUntilUrlAndTitleEquals(const GURL& expected_url,
                                 base::StringPiece expected_title);

  // Calls RunUntilNavigationStateMatches with a NavigationState that has
  // the error page type, |expected_title|, and the main document loaded.
  void RunUntilErrorPageIsLoadedAndTitleEquals(
      base::StringPiece expected_title);

  // Calls RunUntilNavigationStateMatches with a NavigationState that has
  // all the expected fields and the normal page type.
  void RunUntilUrlTitleBackForwardEquals(const GURL& expected_url,
                                         base::StringPiece expected_title,
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
  // fuchsia::web::NavigationEventListener implementation.
  void OnNavigationStateChanged(
      fuchsia::web::NavigationState change,
      OnNavigationStateChangedCallback callback) override;

  // Compare the current state with all fields of |expected| that have been set.
  bool AllFieldsMatch(const fuchsia::web::NavigationState& expected);

  void QuitLoopIfAllFieldsMatch(
      const fuchsia::web::NavigationState* expected_state,
      base::RepeatingClosure quit_run_loop_closure,
      BeforeAckCallback before_ack_callback,
      const fuchsia::web::NavigationState& change,
      fuchsia::web::NavigationEventListener::OnNavigationStateChangedCallback
          ack_callback);

  fuchsia::web::NavigationState current_state_;
  fuchsia::web::NavigationState last_changes_;

  BeforeAckCallback before_ack_;
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_TEST_TEST_NAVIGATION_LISTENER_H_
