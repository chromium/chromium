// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

#import <Foundation/Foundation.h>

class ChromeBrowserState;
class GURL;

namespace send_tab_to_self {

// Returns true if the SendTabToSelf sync datatype is active.
bool IsUserSyncTypeActive(ChromeBrowserState* browser_state);

// Returns true if there is valid device.
bool HasValidTargetDevice(ChromeBrowserState* browser_state);

// Returns true if the tab and web content requirements are met:
//  User is viewing an HTTP or HTTPS page.
//  User is not on a native page.
//  User is not in Incongnito mode.
bool AreContentRequirementsMet(const GURL& gurl,
                               ChromeBrowserState* browser_state);

// Returns true if all conditions are true and shows the option onto the menu.
bool ShouldOfferFeature(ChromeBrowserState* browser_state, const GURL& url);

}  // namespace send_tab_to_self

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
