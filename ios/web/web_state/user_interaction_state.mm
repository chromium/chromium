// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/user_interaction_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The duration of the period following a screen touch during which the user is
// still considered to be interacting with the page.
const NSTimeInterval kMaximumDelayForUserInteractionInSeconds = 2;
}

namespace web {

UserInteractionState::UserInteractionState()
    : user_interaction_registered_since_page_loaded_(false),
      user_interaction_registered_since_last_url_change_(false),
      user_interaction_registered_since_web_view_created_(false),
      tap_in_progress_(false),
      last_user_interaction_(nullptr) {}

UserInteractionState::~UserInteractionState() {}

bool UserInteractionState::UserInteractionRegisteredSincePageLoaded() const {
  return user_interaction_registered_since_page_loaded_;
}

void UserInteractionState::SetUserInteractionRegisteredSincePageLoaded(
    bool user_interaction_registered_since_page_loaded) {
  user_interaction_registered_since_page_loaded_ =
      user_interaction_registered_since_page_loaded;
  if (user_interaction_registered_since_page_loaded) {
    user_interaction_registered_since_last_url_change_ = true;
    user_interaction_registered_since_web_view_created_ = true;
  }
}

bool UserInteractionState::UserInteractionRegisteredSinceLastUrlChange() const {
  return user_interaction_registered_since_last_url_change_;
}

void UserInteractionState::SetUserInteractionRegisteredSinceLastUrlChange(
    bool user_interaction_registered_since_last_url_change) {
  user_interaction_registered_since_last_url_change_ =
      user_interaction_registered_since_last_url_change;
  if (user_interaction_registered_since_last_url_change) {
    user_interaction_registered_since_web_view_created_ = true;
  }
}

bool UserInteractionState::UserInteractionRegisteredSinceWebViewCreated()
    const {
  return user_interaction_registered_since_web_view_created_;
}

void UserInteractionState::SetTapInProgress(bool tap_in_progress) {
  tap_in_progress_ = tap_in_progress;
}

void UserInteractionState::ResetLastTransferTime() {
  last_transfer_time_in_seconds_ = CFAbsoluteTimeGetCurrent();
}

web::UserInteractionEvent* UserInteractionState::LastUserInteraction() const {
  return last_user_interaction_.get();
}

void UserInteractionState::SetLastUserInteraction(
    std::unique_ptr<web::UserInteractionEvent> last_user_interaction) {
  last_user_interaction_ = std::move(last_user_interaction);
}

bool UserInteractionState::HasUserTappedRecently(WKWebView* web_view) const {
  // Scrolling generates a pair of touch on/off event which causes
  // last_user_interaction_ to register that there was user interaction. Checks
  // for scrolling first to override time-based tap heuristics.
  if (web_view.scrollView.dragging || web_view.scrollView.decelerating)
    return NO;
  if (!last_user_interaction_)
    return NO;
  return tap_in_progress_ ||
         ((CFAbsoluteTimeGetCurrent() - last_user_interaction_->time) <
          kMaximumDelayForUserInteractionInSeconds);
}

bool UserInteractionState::IsUserInteracting(WKWebView* web_view) const {
  // If page transfer started after last tap, user is deemed to be no longer
  // interacting.
  if (!last_user_interaction_ ||
      last_transfer_time_in_seconds_ > last_user_interaction_->time) {
    return NO;
  }
  return HasUserTappedRecently(web_view);
}

}  // namespace web
