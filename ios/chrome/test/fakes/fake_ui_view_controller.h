// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_UI_VIEW_CONTROLLER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_UI_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// UIViewController used for testing.
// This class updates the object state instead of actually presenting and
// waiting for animation.
@interface FakeUIViewController : UIViewController
// Updated with view controller to present when presentViewController is called.
@property(nonatomic, strong) UIViewController* presentedViewController;
@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_UI_VIEW_CONTROLLER_H_
