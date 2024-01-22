// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_USER_INTERFACE_STYLE_RECORDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_USER_INTERFACE_STYLE_RECORDER_H_

#import <UIKit/UIKit.h>

// Reports metrics for the user interface style on iOS 13+.
@interface UserInterfaceStyleRecorder : NSObject

- (instancetype)initWithUserInterfaceStyle:(UIUserInterfaceStyle)style
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;


@end

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_USER_INTERFACE_STYLE_RECORDER_H_
