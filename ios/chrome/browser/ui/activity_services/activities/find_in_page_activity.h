// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_FIND_IN_PAGE_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_FIND_IN_PAGE_ACTIVITY_H_

#import <UIKit/UIKit.h>

@protocol FindInPageCommands;
@class ShareToData;

// Activity to trigger the find in page feature.
@interface FindInPageActivity : UIActivity

// Initializes the find in page activity with the given |data| and the
// |handler|.
- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<FindInPageCommands>)handler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_FIND_IN_PAGE_ACTIVITY_H_
