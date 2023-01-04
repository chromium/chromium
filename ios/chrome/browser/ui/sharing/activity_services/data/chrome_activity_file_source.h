// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_FILE_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_FILE_SOURCE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_source.h"

// This UIActivityItemSource-conforming object corresponds to a file. It
// can be used with other Sharing Extensions that handle file. The `filePath` is
// the path where is downloaded the file shared with Sharing Extensions.
@interface ChromeActivityFileSource : NSObject <ChromeActivityItemSource>

// Default initializer. `filePath` must not be nil.
- (instancetype)initWithFilePath:(NSURL*)filePath NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_FILE_SOURCE_H_
