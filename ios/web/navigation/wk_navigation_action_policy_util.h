// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_WK_NAVIGATION_ACTION_POLICY_UTIL_H_
#define IOS_WEB_NAVIGATION_WK_NAVIGATION_ACTION_POLICY_UTIL_H_

#import <WebKit/WebKit.h>

namespace web {

// Navigation action policy which allows the load but prevents opening universal
// links in native applications.
extern const WKNavigationActionPolicy
    kNavigationActionPolicyAllowAndBlockUniversalLinks;

// Returns the WKNavigationActionPolicy for allowing navigations given the
// `block_universal` state for the associated BrowserState.
WKNavigationActionPolicy GetAllowNavigationActionPolicy(bool block_universal);

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_WK_NAVIGATION_ACTION_POLICY_UTIL_H_
