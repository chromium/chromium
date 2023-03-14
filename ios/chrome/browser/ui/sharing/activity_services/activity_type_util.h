// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITY_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITY_TYPE_UTIL_H_

#import <UIKit/UIKit.h>

namespace activity_type_util {

enum ActivityType {
  BOOKMARK,
  COPY,
  NATIVE_FACEBOOK,
  NATIVE_MAIL,
  NATIVE_MESSAGE,
  NATIVE_TWITTER,
  NATIVE_WEIBO,
  NATIVE_CLIPBOARD,
  NATIVE_SAVE_IMAGE,
  NATIVE_PRINT,
  NATIVE_ADD_TO_HOME,
  PRINT,
  FIND_IN_PAGE,
  GOOGLE_DRIVE,
  GOOGLE_GMAIL,
  GOOGLE_GOOGLEPLUS,
  GOOGLE_HANGOUTS,
  GOOGLE_INBOX,
  GOOGLE_UNKNOWN,
  READ_LATER,
  REQUEST_DESKTOP_MOBILE_SITE,
  THIRD_PARTY_MAILBOX,
  THIRD_PARTY_FACEBOOK_MESSENGER,
  THIRD_PARTY_WHATS_APP,
  THIRD_PARTY_LINE,
  THIRD_PARTY_VIBER,
  THIRD_PARTY_SKYPE,
  THIRD_PARTY_TANGO,
  THIRD_PARTY_WECHAT,
  THIRD_PARTY_EVERNOTE,
  THIRD_PARTY_PINTEREST,
  THIRD_PARTY_POCKET,
  THIRD_PARTY_READABILITY,
  THIRD_PARTY_INSTAPAPER,
  SEND_TAB_TO_SELF,
  GENERATE_QR_CODE,
  NATIVE_SAVE_FILE,
  NATIVE_MARKUP,
  // UNKNOWN must be the last type.
  UNKNOWN,
};

// Returns the ActivityType enum associated with `activityString`, which is the
// bundle ID of a iOS App Extension. Returns UNKNOWN if `activityString` does
// match any known App Extensions. `activityString` must not be nil.
ActivityType TypeFromString(NSString* activityString);

// Records the UMA for activity `type`.
void RecordMetricForActivity(ActivityType type);

}  // namespace activity_type_util

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITY_TYPE_UTIL_H_
