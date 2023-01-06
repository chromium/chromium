// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_WK_CONTENT_RULE_LIST_UTIL_H_
#define IOS_WEB_WEB_STATE_UI_WK_CONTENT_RULE_LIST_UTIL_H_

#import <Foundation/Foundation.h>

namespace web {

// Creates the rules json as a string for blocking all online resources loaded
// from file:// or application specific scheme:// pages.
NSString* CreateLocalBlockingJsonRuleList();

// Creates the rules json as a string for autoupgrading mixed content on
// HTTPS URLs.
NSString* CreateMixedContentAutoUpgradeJsonRuleList();

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_WK_CONTENT_RULE_LIST_UTIL_H_
