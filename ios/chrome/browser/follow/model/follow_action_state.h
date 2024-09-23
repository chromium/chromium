// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_ACTION_STATE_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_ACTION_STATE_H_

#import <Foundation/Foundation.h>

// The state of the "Follow" action. e.g. The state the Follow button in the
// Overflow menu.
typedef NS_ENUM(NSInteger, FollowActionState) {
  // "Follow" action is hidden.
  FollowActionStateHidden,
  // "Follow" action is shown but disabled.
  FollowActionStateDisabled,
  // "Follow" action is shown and enabled.
  FollowActionStateEnabled,
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_ACTION_STATE_H_
