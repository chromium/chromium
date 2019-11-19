// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_USER_INTERACTION_STATE_H_
#define IOS_WEB_WEB_STATE_USER_INTERACTION_STATE_H_

#import <WebKit/WebKit.h>
#include <memory>

#import "ios/web/web_state/user_interaction_event.h"

namespace web {

// Records user's interaction state with web content.
class UserInteractionState {
 public:
  UserInteractionState();
  ~UserInteractionState();

  // Returns |user_interaction_registered_since_page_loaded_|.
  bool UserInteractionRegisteredSincePageLoaded() const;
  // Sets |user_interaction_registered_since_page_loaded_|. If true, also sets
  // |user_interaction_registered_since_last_url_change_| and
  // |user_interaction_registered_since_web_view_created_| to true;
  void SetUserInteractionRegisteredSincePageLoaded(
      bool user_interaction_registered_since_page_loaded);

  // Returns |user_interaction_registered_since_last_url_change_|.
  bool UserInteractionRegisteredSinceLastUrlChange() const;
  // Sets |user_interaction_registered_since_last_url_change_|. If true, also
  // sets |user_interaction_registered_since_web_view_created_| to true.
  void SetUserInteractionRegisteredSinceLastUrlChange(
      bool interaction_registered_since_last_url_change);

  // Returns |user_interaction_registered_since_web_view_created_|.
  bool UserInteractionRegisteredSinceWebViewCreated() const;

  // Sets |tap_in_progress_|.
  void SetTapInProgress(bool tap_in_progress);

  // Resets |last_transfer_time_in_seconds_| to current time.
  void ResetLastTransferTime();

  // Returns the raw pointer managed by |last_user_interaction_|.
  web::UserInteractionEvent* LastUserInteraction() const;
  // Sets |last_user_interaction_|.
  void SetLastUserInteraction(
      std::unique_ptr<web::UserInteractionEvent> last_user_interaction);

  // Returns true if the user interacted with the page recently.
  bool HasUserTappedRecently(WKWebView* web_view) const;
  // Returns whether the user is interacting with the page.
  bool IsUserInteracting(WKWebView* web_view) const;

 private:
  // Whether a user interaction has been registered since the page has loaded.
  bool user_interaction_registered_since_page_loaded_;
  // Whether a user interaction has been registered since the last URL change.
  bool user_interaction_registered_since_last_url_change_;
  // Whether a user interaction has been registered since the web view is
  // created.
  bool user_interaction_registered_since_web_view_created_;
  // Whether a tap is in progress.
  bool tap_in_progress_;
  // The time of the last page transfer start, measured in seconds since Jan 1
  // 2001.
  CFAbsoluteTime last_transfer_time_in_seconds_;
  // Data on the recorded last user interaction.
  std::unique_ptr<web::UserInteractionEvent> last_user_interaction_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_USER_INTERACTION_STATE_H_
