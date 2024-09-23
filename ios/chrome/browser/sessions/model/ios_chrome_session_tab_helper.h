// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_SESSION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_SESSION_TAB_HELPER_H_

#include "components/sessions/core/session_id.h"
#include "ios/web/public/web_state_user_data.h"

class IOSChromeSessionTabHelper
    : public web::WebStateUserData<IOSChromeSessionTabHelper> {
 public:
  IOSChromeSessionTabHelper(const IOSChromeSessionTabHelper&) = delete;
  IOSChromeSessionTabHelper& operator=(const IOSChromeSessionTabHelper&) =
      delete;

  ~IOSChromeSessionTabHelper() override;

  // Returns the identifier used by session restore for this tab.
  SessionID session_id() const { return session_id_; }

  // Identifier of the window the tab is in.
  void SetWindowID(SessionID window_id);
  SessionID window_id() const { return window_id_; }

 private:
  explicit IOSChromeSessionTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<IOSChromeSessionTabHelper>;

  // Unique identifier of the tab for session restore. It is stable across
  // application restart as it is set to WebState::GetUniqueIdentifier().
  const SessionID session_id_;

  // Unique identifier of the window the tab is in.
  SessionID window_id_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_SESSION_TAB_HELPER_H_
