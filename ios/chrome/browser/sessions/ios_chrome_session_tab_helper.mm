// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeSessionTabHelper::IOSChromeSessionTabHelper(web::WebState* web_state)
    : session_id_(SessionID::NewUnique()),
      window_id_(SessionID::InvalidValue()) {}

IOSChromeSessionTabHelper::~IOSChromeSessionTabHelper() {}

void IOSChromeSessionTabHelper::SetWindowID(const SessionID& id) {
  window_id_ = id;
}

WEB_STATE_USER_DATA_KEY_IMPL(IOSChromeSessionTabHelper)
