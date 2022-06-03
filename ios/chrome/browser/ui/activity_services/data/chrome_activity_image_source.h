// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_IMAGE_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_IMAGE_SOURCE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_item_source.h"

// Returns an image to the UIActivities that can take advantage of it.
@interface ChromeActivityImageSource : NSObject <ChromeActivityItemSource>

// Default initializer. |image| and |title| must not be nil.
- (instancetype)initWithImage:(UIImage*)image title:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_IMAGE_SOURCE_H_
