// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_PHYSICAL_KEYBOARD_DETECTOR_H_
#define REMOTING_IOS_APP_PHYSICAL_KEYBOARD_DETECTOR_H_

#import <UIKit/UIKit.h>

// A class for detecting whether an physical keyboard is presented.
@interface PhysicalKeyboardDetector : NSObject

// |callback| will be called with YES if an physical keyboard is presented.
// Note that you'll need to manually restore the first responder after the
// detection is done.
+ (void)detectOnView:(UIView*)view callback:(void (^)(BOOL))callback;

@end

#endif  // REMOTING_IOS_APP_PHYSICAL_KEYBOARD_DETECTOR_H_
