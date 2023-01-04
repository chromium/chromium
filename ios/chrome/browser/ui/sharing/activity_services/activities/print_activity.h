// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_PRINT_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_PRINT_ACTIVITY_H_

#import <UIKit/UIKit.h>

@protocol BrowserCoordinatorCommands;
@class ShareImageData;
@class ShareToData;

// Activity that triggers the printing service.
@interface PrintActivity : UIActivity

// Initializes the print activity with the given tab `data` and the `handler`.
// Print preview will be presented on top of `baseViewController`.
- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<BrowserCoordinatorCommands>)handler
          baseViewController:(UIViewController*)baseViewController
    NS_DESIGNATED_INITIALIZER;
// Initializes the print activity with the given `imageData` and the `handler`.
// Print preview will be presented on top of `baseViewController`.
- (instancetype)initWithImageData:(ShareImageData*)imageData
                          handler:(id<BrowserCoordinatorCommands>)handler
               baseViewController:(UIViewController*)baseViewController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_PRINT_ACTIVITY_H_
