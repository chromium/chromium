// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_DELEGATE_H_

// Protocol to communicate Badge actions to the mediator.
@protocol BadgeDelegate
// Action when a Passwords badge is tapped.
- (void)passwordsBadgeButtonTapped:(id)sender;

// Action when a Save Card badge is tapped.
- (void)saveCardBadgeButtonTapped:(id)sender;

// Action when a Translate badge is tapped.
- (void)translateBadgeButtonTapped:(id)sender;

// Action when the overflow badge is tapped.
- (void)overflowBadgeButtonTapped:(id)sender;
@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_DELEGATE_H_
