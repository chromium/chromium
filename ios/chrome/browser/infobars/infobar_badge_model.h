// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_MODEL_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_MODEL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"

// A model object that represents a badge for an Infobar.
@interface InfobarBadgeModel : NSObject <BadgeItem>

- (instancetype)initWithInfobarType:(InfobarType)type NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_MODEL_H_
