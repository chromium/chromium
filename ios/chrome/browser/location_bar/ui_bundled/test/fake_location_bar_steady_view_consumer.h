// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_TEST_FAKE_LOCATION_BAR_STEADY_VIEW_CONSUMER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_TEST_FAKE_LOCATION_BAR_STEADY_VIEW_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view_consumer.h"

@interface FakeLocationBarSteadyViewConsumer
    : NSObject <LocationBarSteadyViewConsumer>
@property(nonatomic, strong, readonly) NSString* locationText;
@property(nonatomic, assign, readonly) BOOL clipTail;
@property(nonatomic, strong, readonly) UIImage* icon;
@property(nonatomic, strong, readonly) NSString* statusText;
@property(nonatomic, assign, readonly, getter=isLocationShareable)
    BOOL locationShareable;
@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_TEST_FAKE_LOCATION_BAR_STEADY_VIEW_CONSUMER_H_
