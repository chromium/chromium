// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_TEST_NAVIGATION_LISTENER_H_
#define FUCHSIA_BASE_TEST_NAVIGATION_LISTENER_H_

#include <fuchsia/web/cpp/fidl.h>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "url/gurl.h"

namespace cr_fuchsia {

// Observes navigation events and enables test code to block until a desired
// navigational state is observed.
class TestNavigationListener : public fuchsia::web::NavigationEventListener {
 public:
  using BeforeAckCallback =
      base::RepeatingCallback<void(const fuchsia::web::NavigationState& change,
                                   OnNavigationStateChangedCallback)>;

  TestNavigationListener();
  ~TestNavigationListener() final;

  // Spins a RunLoop until the navigation state of the page matches the fields
  // of |expected_state| that have been set.
  void RunUntilNavigationStateMatches(
      const fuchsia::web::NavigationState& expected_state);

  // Calls RunUntilNavigationStateMatches with a NagivationState that has
  // |expected_url|.
  void RunUntilUrlEquals(const GURL& expected_url);

  // Calls RunUntilNavigationStateMatches with a NagivationState that has
  // |expected_url| and |expected_title|.
  void RunUntilUrlAndTitleEquals(const GURL& expected_url,
                                 base::StringPiece expected_title);

  // Calls RunUntilNavigationStateMatches with a NagivationState that has
  // all the expected fields.
  void RunUntilUrlTitleBackForwardEquals(const GURL& expected_url,
                                         base::StringPiece expected_title,
                                         bool expected_can_go_back,
                                         bool expected_can_go_forward);

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
      OnNavigationStateChangedCallback callback) final;

  fuchsia::web::NavigationState current_state_;

  BeforeAckCallback before_ack_;

  // Compare the current state with all fields of |expected| that have been set.
  bool AllFieldsMatch(const fuchsia::web::NavigationState& expected);

  DISALLOW_COPY_AND_ASSIGN(TestNavigationListener);
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_TEST_NAVIGATION_LISTENER_H_
