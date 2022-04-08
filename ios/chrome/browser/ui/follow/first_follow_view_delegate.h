// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FOLLOW_FIRST_FOLLOW_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_FOLLOW_FIRST_FOLLOW_VIEW_DELEGATE_H_

// Actions triggered in the first follow UI.
@protocol FirstFollowViewDelegate

// Go To Feed button tapped.
- (void)handleGoToFeedTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_FOLLOW_FIRST_FOLLOW_VIEW_DELEGATE_H_
