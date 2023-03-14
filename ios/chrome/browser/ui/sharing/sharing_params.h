// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_SHARING_PARAMS_H_
#define IOS_CHROME_BROWSER_UI_SHARING_SHARING_PARAMS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/ui/sharing/sharing_scenario.h"

class GURL;

// Parameter object used to configure the activity service scenario.
@interface SharingParams : NSObject

// Initializes an instance configured to share the current tab's URL for the
// metrics `scenario`.
- (instancetype)initWithScenario:(SharingScenario)scenario
    NS_DESIGNATED_INITIALIZER;

// Initializes an instance configured to share an `image`, along
// with its `title`, for the metrics `scenario`.
- (instancetype)initWithImage:(UIImage*)image
                        title:(NSString*)title
                     scenario:(SharingScenario)scenario;

// Initializes an instance configured to share `URL`, along with its `title` for
// the metrics `scenario`.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
                   scenario:(SharingScenario)scenario;

// Initializes an instance configured to share one or more URLs represented by
// `URLWithTitle`s, for the metrics `scenario`.
- (instancetype)initWithURLs:(NSArray<URLWithTitle*>*)URLs
                    scenario:(SharingScenario)scenario;

// Initializes an instance configured to share an `URL`, along
// with its `title` and `additionalText`, for the metrics `scenario`.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
             additionalText:(NSString*)additionalText
                   scenario:(SharingScenario)scenario;

- (instancetype)init NS_UNAVAILABLE;

// Image to be shared.
@property(nonatomic, readonly, strong) UIImage* image;

// Title of the content that will be shared. Must be set if `image` is set.
@property(nonatomic, readonly, copy) NSString* imageTitle;

// URLs, and associated titles, of the page(s) to be shared.
@property(nonatomic, readonly) NSArray<URLWithTitle*>* URLs;

// Any additional text to be shared along with the page's details. May be nil.
@property(nonatomic, readonly, copy) NSString* additionalText;

// Current sharing scenario.
@property(nonatomic, readonly, assign) SharingScenario scenario;

// File to be shared.
@property(nonatomic, strong) NSURL* filePath;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_SHARING_PARAMS_H_
