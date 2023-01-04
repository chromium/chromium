// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_SHARE_FILE_DATA_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_SHARE_FILE_DATA_H_

#import <UIKit/UIKit.h>

// Data object used to represent a file that will be shared via the activity
// view.
@interface ShareFileData : NSObject

// Designated initializer.
- (instancetype)initWithFilePath:(NSURL*)filePath NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// File to be shared.
@property(nonatomic, readonly) NSURL* filePath;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_SHARE_FILE_DATA_H_
