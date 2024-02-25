// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

@protocol BadgeItem;
enum class InfobarType;

namespace web {
class WebState;
}

// Delegate used by InfobarBadgeTabHelper to manage the Infobar badges.
@protocol InfobarBadgeTabHelperDelegate

// Checks whether badge is supported for `infobarType`.
- (BOOL)badgeSupportedForInfobarType:(InfobarType)infobarType;

// Ask the delegate to rerender the infobar badges, as the list of badges and/or
// their states may have changed.
- (void)updateBadgesShownForWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_
