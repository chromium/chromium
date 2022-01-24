// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/test/test_navigation_listener.h"

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"

namespace cr_fuchsia {

TestNavigationListener::TestNavigationListener() {
  // Set up the default acknowledgement handling behavior.
  SetBeforeAckHook({});
}

TestNavigationListener::~TestNavigationListener() = default;

void TestNavigationListener::RunUntilNavigationStateMatches(
    const fuchsia::web::NavigationState& expected_state) {
  DCHECK(before_ack_);

  // Spin the runloop until the expected conditions are met.
  if (AllFieldsMatch(expected_state))
    return;

  base::RunLoop run_loop;
  base::AutoReset<BeforeAckCallback> callback_setter(
      &before_ack_,
      base::BindRepeating(&TestNavigationListener::QuitLoopIfAllFieldsMatch,
                          base::Unretained(this),
                          base::Unretained(&expected_state),
                          run_loop.QuitClosure(), before_ack_));
  run_loop.Run();
}

void TestNavigationListener::RunUntilLoaded() {
  fuchsia::web::NavigationState state;
  state.set_page_type(fuchsia::web::PageType::NORMAL);
  state.set_is_main_document_loaded(true);
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::RunUntilUrlEquals(const GURL& expected_url) {
  fuchsia::web::NavigationState state;
  state.set_url(expected_url.spec());
  state.set_page_type(fuchsia::web::PageType::NORMAL);
  state.set_is_main_document_loaded(true);
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::RunUntilTitleEquals(
    const base::StringPiece expected_title) {
  fuchsia::web::NavigationState state;
  state.set_title(std::string(expected_title));
  state.set_page_type(fuchsia::web::PageType::NORMAL);
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::RunUntilUrlAndTitleEquals(
    const GURL& expected_url,
    const base::StringPiece expected_title) {
  fuchsia::web::NavigationState state;
  state.set_url(expected_url.spec());
  state.set_title(std::string(expected_title));
  state.set_page_type(fuchsia::web::PageType::NORMAL);
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::RunUntilErrorPageIsLoadedAndTitleEquals(
    base::StringPiece expected_title) {
  fuchsia::web::NavigationState state;
  state.set_title(std::string(expected_title));
  state.set_page_type(fuchsia::web::PageType::ERROR);
  state.set_is_main_document_loaded(true);
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::RunUntilUrlTitleBackForwardEquals(
    const GURL& expected_url,
    base::StringPiece expected_title,
    bool expected_can_go_back,
    bool expected_can_go_forward) {
  fuchsia::web::NavigationState state;
  state.set_url(expected_url.spec());
  state.set_title(std::string(expected_title));
  state.set_page_type(fuchsia::web::PageType::NORMAL);
  state.set_can_go_back(expected_can_go_back);
  state.set_can_go_forward(expected_can_go_forward);
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::SetBeforeAckHook(BeforeAckCallback send_ack_cb) {
  if (send_ack_cb) {
    before_ack_ = send_ack_cb;
  } else {
    before_ack_ = base::BindRepeating(
        [](const fuchsia::web::NavigationState&,
           OnNavigationStateChangedCallback callback) { callback(); });
  }
}

void TestNavigationListener::OnNavigationStateChanged(
    fuchsia::web::NavigationState changes,
    OnNavigationStateChangedCallback callback) {
  DCHECK(before_ack_);

  // Update our local cache of the Frame's current state.
  if (changes.has_url())
    current_state_.set_url(changes.url());
  if (changes.has_title())
    current_state_.set_title(changes.title());
  if (changes.has_can_go_back())
    current_state_.set_can_go_back(changes.can_go_back());
  if (changes.has_can_go_forward())
    current_state_.set_can_go_forward(changes.can_go_forward());
  if (changes.has_page_type())
    current_state_.set_page_type(changes.page_type());
  if (changes.has_is_main_document_loaded()) {
    current_state_.set_is_main_document_loaded(
        changes.is_main_document_loaded());
  }
  if (changes.has_favicon()) {
    current_state_.set_favicon(std::move(*changes.mutable_favicon()));
  }

  if (VLOG_IS_ON(1)) {
    std::string state_string;
    state_string.reserve(100);

    if (current_state_.has_url()) {
      state_string.append(
          base::StringPrintf(" url=%s ", current_state_.url().c_str()));
    }

    if (current_state_.has_title()) {
      state_string.append(
          base::StringPrintf(" title='%s' ", current_state_.title().c_str()));
    }

    if (current_state_.has_can_go_back()) {
      state_string.append(
          base::StringPrintf(" can_go_back=%d ", current_state_.can_go_back()));
    }

    if (current_state_.has_can_go_forward()) {
      state_string.append(base::StringPrintf(" can_go_forward=%d ",
                                             current_state_.can_go_forward()));
    }

    if (current_state_.has_page_type()) {
      state_string.append(base::StringPrintf(
          " page_type=%s ",
          (current_state_.page_type() == fuchsia::web::PageType::NORMAL
               ? "NORMAL"
               : "ERROR")));
    }

    if (current_state_.has_is_main_document_loaded()) {
      state_string.append(
          base::StringPrintf(" is_main_document_loaded=%d ",
                             current_state_.is_main_document_loaded()));
    }

    VLOG(1) << "Navigation state changed: " << state_string;
  }

  last_changes_ = std::move(changes);

  // Signal readiness for the next navigation event.
  before_ack_.Run(last_changes_, std::move(callback));
}

bool TestNavigationListener::AllFieldsMatch(
    const fuchsia::web::NavigationState& expected) {
  if (expected.has_url() &&
      (!current_state_.has_url() || expected.url() != current_state_.url())) {
    return false;
  }

  if (expected.has_title() && (!current_state_.has_title() ||
                               expected.title() != current_state_.title())) {
    return false;
  }

  if (expected.has_can_go_forward() &&
      (!current_state_.has_can_go_forward() ||
       expected.can_go_forward() != current_state_.can_go_forward())) {
    return false;
  }

  if (expected.has_can_go_back() &&
      (!current_state_.has_can_go_back() ||
       expected.can_go_back() != current_state_.can_go_back())) {
    return false;
  }

  if (expected.has_page_type() &&
      (!current_state_.has_page_type() ||
       expected.page_type() != current_state_.page_type())) {
    return false;
  }

  if (expected.has_is_main_document_loaded() &&
      (!current_state_.has_is_main_document_loaded() ||
       expected.is_main_document_loaded() !=
           current_state_.is_main_document_loaded())) {
    return false;
  }

  return true;
}

void TestNavigationListener::QuitLoopIfAllFieldsMatch(
    const fuchsia::web::NavigationState* expected_state,
    base::RepeatingClosure quit_run_loop_closure,
    TestNavigationListener::BeforeAckCallback before_ack_callback,
    const fuchsia::web::NavigationState& change,
    fuchsia::web::NavigationEventListener::OnNavigationStateChangedCallback
        ack_callback) {
  if (AllFieldsMatch(*expected_state))
    quit_run_loop_closure.Run();
  before_ack_callback.Run(change, std::move(ack_callback));
}

}  // namespace cr_fuchsia
