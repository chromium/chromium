// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_USER_INTERFACE_STYLE_RECORDER_H_
#define IOS_CHROME_BROWSER_METRICS_USER_INTERFACE_STYLE_RECORDER_H_

#import <UIKit/UIKit.h>

// Reports metrics for the user interface style on iOS 13+.
API_AVAILABLE(ios(13.0))
@interface UserInterfaceStyleRecorder : NSObject

- (instancetype)initWithUserInterfaceStyle:(UIUserInterfaceStyle)style
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Call this method to report the new user interface to UMA.
- (void)userInterfaceStyleDidChange:(UIUserInterfaceStyle)newUserInterfaceStyle;

@end

#endif  // IOS_CHROME_BROWSER_METRICS_USER_INTERFACE_STYLE_RECORDER_H_
