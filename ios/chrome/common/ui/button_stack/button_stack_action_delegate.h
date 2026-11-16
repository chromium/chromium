// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_ACTION_DELEGATE_H_
#define IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_ACTION_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate for handling actions from a ButtonStackViewController.
@protocol ButtonStackActionDelegate <NSObject>

// Called when the primary action button is tapped.
- (void)didTapPrimaryActionButton;

// Called when the secondary action button is tapped.
- (void)didTapSecondaryActionButton;

// Called when the tertiary action button is tapped.
- (void)didTapTertiaryActionButton;

// Called when the `ButtonStackViewController` disappears because it is popped,
// or because it or one of its ancestors (e.g. its navigation controller) is
// being dismissed. It is not called when an ancestor is popped (e.g. when the
// `ButtonStackViewController` is a child of a popped view controller); that
// ancestor is responsible for handling it. This is also called for
// programmatic dismissals, so clear the `actionDelegate` before dismissing to
// avoid re-entrancy if needed. It is not called if the view controller is
// already offscreen when removed.
- (void)didDismissButtonStackViewController;

@end

#endif  // IOS_CHROME_COMMON_UI_BUTTON_STACK_BUTTON_STACK_ACTION_DELEGATE_H_
