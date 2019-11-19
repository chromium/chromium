// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_TEST_FAKE_LOCATION_BAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_TEST_FAKE_LOCATION_BAR_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/location_bar/location_bar_consumer.h"

@interface FakeLocationBarConsumer : NSObject <LocationBarConsumer>
@property(nonatomic, strong, readonly) NSString* locationText;
@property(nonatomic, assign, readonly) BOOL clipTail;
@property(nonatomic, strong, readonly) UIImage* icon;
@property(nonatomic, strong, readonly) NSString* statusText;
@property(nonatomic, assign, readonly, getter=isLocationShareable)
    BOOL locationShareable;
@property(nonatomic, assign, readonly, getter=isSearchByImageSupported)
    BOOL searchByImageSupported;
@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_TEST_FAKE_LOCATION_BAR_CONSUMER_H_
