// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_WK_NAVIGATION_ACTION_UTIL_H_
#define IOS_WEB_NAVIGATION_WK_NAVIGATION_ACTION_UTIL_H_

@class WKNavigationAction;
@class NSString;

namespace web {

// This enum values indicates whether a WKNavigationAction was initiated by the
// user or initiated by a script.
enum class NavigationActionInitiationType {
  // This is the default value for the enum, but it will also be the case when
  // there is no way to detect if the navigationAction initiator by examining
  // the WKNavigationAction fields.
  kUnknownInitiator = 0,
  // The navigation action is a link click initiated by the user.
  kUserInitiated,
};

// Returns the WKNavigationAction initiation type.
NavigationActionInitiationType GetNavigationActionInitiationType(
    WKNavigationAction* action);

// Returns theNavigationIniationType based on the navigationAction description
// string when voiceover is off.
NavigationActionInitiationType
GetNavigationActionInitiationTypeWithVoiceOverOff(NSString* action_description);

// Returns theNavigationIniationType based on the navigationAction description
// string when voiceover is on.
NavigationActionInitiationType GetNavigationActionInitiationTypeWithVoiceOverOn(
    NSString* action_description);

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_WK_NAVIGATION_ACTION_UTIL_H_
