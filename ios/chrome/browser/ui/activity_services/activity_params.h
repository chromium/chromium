// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_PARAMS_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_PARAMS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/activity_services/activity_scenario.h"

class GURL;

// Parameter object used to configure the activity service scenario.
@interface ActivityParams : NSObject

// Initializes an instance configured to share the current tab's URL for the
// metrics |scenario|.
- (instancetype)initWithScenario:(ActivityScenario)scenario
    NS_DESIGNATED_INITIALIZER;

// Initializes an instance configured to share an |image|, along
// with its |title|, for the metrics |scenario|.
- (instancetype)initWithImage:(UIImage*)image
                        title:(NSString*)title
                     scenario:(ActivityScenario)scenario;

// Initializes an instance configured to share an |URL|, along
// with its |title|, for the metrics |scenario|.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
                   scenario:(ActivityScenario)scenario;

// Initializes an instance configured to share an |URL|, along
// with its |title| and |additionalText|, for the metrics |scenario|.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
             additionalText:(NSString*)additionalText
                   scenario:(ActivityScenario)scenario;

- (instancetype)init NS_UNAVAILABLE;

// Image to be shared.
@property(nonatomic, readonly, strong) UIImage* image;

// URL of a page to be shared.
@property(nonatomic, readonly, assign) GURL URL;

// Title of the content that will be shared. Must be set if |image| or |URL| are
// set.
@property(nonatomic, readonly, copy) NSString* title;

// Any additional text to be shared along with the page's details. May be nil.
@property(nonatomic, readonly, copy) NSString* additionalText;

// Current sharing scenario.
@property(nonatomic, readonly, assign) ActivityScenario scenario;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_PARAMS_H_
