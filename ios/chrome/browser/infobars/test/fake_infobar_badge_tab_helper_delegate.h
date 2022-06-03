// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_TEST_FAKE_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_TEST_FAKE_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#import "ios/chrome/browser/ui/badges/badge_type.h"

// Fake version of InfobarTabHelperDelegate to use in tests.
@interface FakeInfobarTabHelperDelegate
    : NSObject <InfobarBadgeTabHelperDelegate>
// Returns the BadgeItem that was added to the tab helper for |type|, or nil if
// one does not exist.
- (id<BadgeItem>)itemForBadgeType:(BadgeType)type;
@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_TEST_FAKE_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_
