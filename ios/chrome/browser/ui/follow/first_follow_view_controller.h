// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FOLLOW_FIRST_FOLLOW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FOLLOW_FIRST_FOLLOW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// The UI that informs the user about the feed and following channels after the
// first few times the user follows any channel.
@interface FirstFollowViewController : UIViewController

// The Web Channel title to be shown in the modal.
@property(nonatomic, copy) NSString* webChannelTitle;

@end

#endif  // IOS_CHROME_BROWSER_UI_FOLLOW_FIRST_FOLLOW_VIEW_CONTROLLER_H_
