// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

// Base delegate protocol for the base first run screen view controller to
// communicate with screen-specific coordinators. Only the shared action buttons
// are included in this base protocol; screens with additional buttons should
// extend this protocol.
@protocol FirstRunScreenViewControllerDelegate <NSObject>

@optional

// Invoked when the primary action button is tapped.
- (void)didTapPrimaryActionButton;

// Invoked when the secondary action button is tapped.
- (void)didTapSecondaryActionButton;

// Invoked when the tertiary action button is tapped.
- (void)didTapTertiaryActionButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_SCREEN_VIEW_CONTROLLER_DELEGATE_H_