// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_TEST_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_TEST_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/model/infobar_type.h"

// Test version of InfobarTabHelperDelegate to use in tests.
@interface TestInfobarTabHelperDelegate
    : NSObject <InfobarBadgeTabHelperDelegate>
// Tab helper used in tests.
@property(nonatomic) InfobarBadgeTabHelper* badgeTabHelper;
// Returns the BadgeItem that was added to the tab helper for `type`, or nil if
// one does not exist.
- (id<BadgeItem>)itemForInfobarType:(InfobarType)type;
@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_TEST_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_
