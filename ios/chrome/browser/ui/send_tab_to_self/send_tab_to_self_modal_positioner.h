// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_POSITIONER_H_
#define IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_POSITIONER_H_

#import <UIKit/UIKit.h>

// SendTabToSelfModalPositioner contains methods used to position the send tab
// to self modal dialog.
@protocol SendTabToSelfModalPositioner

// The target height for the modal view to be presented.
- (CGFloat)modalHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_POSITIONER_H_
