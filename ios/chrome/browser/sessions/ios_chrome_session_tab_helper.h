// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_IOS_CHROME_SESSION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SESSIONS_IOS_CHROME_SESSION_TAB_HELPER_H_

#include "base/macros.h"
#include "components/sessions/core/session_id.h"
#import "ios/web/public/web_state_user_data.h"

class IOSChromeSessionTabHelper
    : public web::WebStateUserData<IOSChromeSessionTabHelper> {
 public:
  ~IOSChromeSessionTabHelper() override;

  // Returns the identifier used by session restore for this tab.
  const SessionID& session_id() const { return session_id_; }

  // Identifier of the window the tab is in.
  void SetWindowID(const SessionID& id);
  const SessionID& window_id() const { return window_id_; }

 private:
  explicit IOSChromeSessionTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<IOSChromeSessionTabHelper>;

  // Unique identifier of the tab for session restore. This id is only unique
  // within the current session, and is not guaranteed to be unique across
  // sessions.
  const SessionID session_id_;

  // Unique identifier of the window the tab is in.
  SessionID window_id_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(IOSChromeSessionTabHelper);
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_IOS_CHROME_SESSION_TAB_HELPER_H_
