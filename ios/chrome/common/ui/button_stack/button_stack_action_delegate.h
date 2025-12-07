// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_ACTION_DELEGATE_H_
#define IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_ACTION_DELEGATE_H_

// Delegate for handling actions from a ButtonStackViewController.
@protocol ButtonStackActionDelegate

// Called when the primary action button is tapped.
- (void)didTapPrimaryActionButton;

// Called when the secondary action button is tapped.
- (void)didTapSecondaryActionButton;

// Called when the tertiary action button is tapped.
- (void)didTapTertiaryActionButton;

@end

#endif  // IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_ACTION_DELEGATE_H_
