// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POLICY_USER_POLICY_USER_POLICY_PROMPT_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_POLICY_USER_POLICY_USER_POLICY_PROMPT_PRESENTER_H_

@protocol UserPolicyPromptPresenter <NSObject>

// Stop presenting the view.
- (void)stopPresenting;

// Show the actvity overlay on the view.
- (void)showActivityOverlay;

// Hide the activity overlay on the view.
- (void)hideActivityOverlay;

@end

#endif  // IOS_CHROME_BROWSER_UI_POLICY_USER_POLICY_USER_POLICY_PROMPT_PRESENTER_H_
