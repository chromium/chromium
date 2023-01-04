// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_TEXT_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_TEXT_SOURCE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_source.h"

// This UIActivityItemSource-conforming object conforms to UTType public.text.
@interface ChromeActivityTextSource : NSObject <ChromeActivityItemSource>

// Default initializer. `text` must not be nil.
- (instancetype)initWithText:(NSString*)text;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_TEXT_SOURCE_H_
