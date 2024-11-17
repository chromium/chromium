// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/test_navigation_listener.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "url/gurl.h"

namespace {

const char* PageTypeToString(fuchsia::web::PageType type) {
  return type == fuchsia::web::PageType::NORMAL ? "NORMAL" : "ERROR";
}

}  // namespace

TestNavigationListener::TestNavigationListener() {
  // Set up the default acknowledgement handling behavior.
  SetBeforeAckHook({});
}

TestNavigationListener::~TestNavigationListener() = default;

void TestNavigationListener::RunUntilNavigationStateMatches(
    const fuchsia::web::NavigationState& expected_state) {
  DCHECK(before_ack_);
  DCHECK(!expected_state_);

  expected_state_ = &expected_state;
  if (!AllFieldsMatch(nullptr)) {
    // Spin the runloop until the expected conditions are met.
    base::RunLoop run_loop;
    base::AutoReset<BeforeAckCallback> callback_setter(
        &before_ack_,
        base::BindRepeating(&TestNavigationListener::QuitLoopIfAllFieldsMatch,
                            base::Unretained(this), run_loop.QuitClosure(),
                            before_ack_));
    run_loop.Run();
  }
  expected_state_ = nullptr;
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
    std::string_view expected_title) {
  fuchsia::web::NavigationState state;
  state.set_title(std::string(expected_title));
  state.set_page_type(fuchsia::web::PageType::NORMAL);
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::RunUntilUrlAndTitleEquals(
    const GURL& expected_url,
    std::string_view expected_title) {
  fuchsia::web::NavigationState state;
  state.set_url(expected_url.spec());
  state.set_title(std::string(expected_title));
  state.set_page_type(fuchsia::web::PageType::NORMAL);
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::RunUntilErrorPageIsLoadedAndTitleEquals(
    std::string_view expected_title) {
  fuchsia::web::NavigationState state;
  state.set_title(std::string(expected_title));
  state.set_page_type(fuchsia::web::PageType::ERROR);
  state.set_is_main_document_loaded(true);
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::RunUntilUrlTitleBackForwardEquals(
    const GURL& expected_url,
    std::string_view expected_title,
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
          " page_type=%s ", PageTypeToString(current_state_.page_type())));
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
    std::vector<FailureReason>* failure_reasons) {
  bool success = true;

  if (expected_state_->has_url() &&
      (!current_state_.has_url() ||
       expected_state_->url() != current_state_.url())) {
    if (failure_reasons) {
      failure_reasons->push_back({"url", expected_state_->url()});
    }
    success = false;
  }

  if (expected_state_->has_title() &&
      (!current_state_.has_title() ||
       expected_state_->title() != current_state_.title())) {
    if (failure_reasons) {
      failure_reasons->push_back({"title", expected_state_->title()});
    }
    success = false;
  }

  if (expected_state_->has_can_go_forward() &&
      (!current_state_.has_can_go_forward() ||
       expected_state_->can_go_forward() != current_state_.can_go_forward())) {
    if (failure_reasons) {
      failure_reasons->push_back(
          {"can_go_forward",
           base::NumberToString(expected_state_->can_go_forward())});
    }
    success = false;
  }

  if (expected_state_->has_can_go_back() &&
      (!current_state_.has_can_go_back() ||
       expected_state_->can_go_back() != current_state_.can_go_back())) {
    if (failure_reasons) {
      failure_reasons->push_back(
          {"can_go_back",
           base::NumberToString(expected_state_->can_go_back())});
    }
    success = false;
  }

  if (expected_state_->has_page_type() &&
      (!current_state_.has_page_type() ||
       expected_state_->page_type() != current_state_.page_type())) {
    if (failure_reasons) {
      failure_reasons->push_back(
          {"page_type", PageTypeToString(expected_state_->page_type())});
    }
    success = false;
  }

  if (expected_state_->has_is_main_document_loaded() &&
      (!current_state_.has_is_main_document_loaded() ||
       expected_state_->is_main_document_loaded() !=
           current_state_.is_main_document_loaded())) {
    if (failure_reasons) {
      failure_reasons->push_back(
          {"is_main_document_loaded",
           base::NumberToString(expected_state_->is_main_document_loaded())});
    }
    success = false;
  }

  return success;
}

void TestNavigationListener::QuitLoopIfAllFieldsMatch(
    base::RepeatingClosure quit_run_loop_closure,
    TestNavigationListener::BeforeAckCallback before_ack_callback,
    const fuchsia::web::NavigationState& change,
    fuchsia::web::NavigationEventListener::OnNavigationStateChangedCallback
        ack_callback) {
  std::vector<FailureReason> failure_reasons;

  if (AllFieldsMatch(VLOG_IS_ON(1) ? &failure_reasons : nullptr)) {
    VLOG(1) << "All navigation expectations satisfied, continuing...";
    quit_run_loop_closure.Run();
  } else if (VLOG_IS_ON(1)) {
    std::vector<std::string> formatted_reasons;
    for (const auto& reason : failure_reasons) {
      formatted_reasons.push_back(base::StringPrintf("%s=%s", reason.field_name,
                                                     reason.expected.c_str()));
    }
    VLOG(1) << "Still waiting on the following unmet expectations: "
            << base::JoinString(formatted_reasons, ", ");
  }

  before_ack_callback.Run(change, std::move(ack_callback));
}
