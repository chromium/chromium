// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_LEGACY_DOWNLOAD_MANAGER_STATE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_LEGACY_DOWNLOAD_MANAGER_STATE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/download/download_manager_state.h"

// View that display relevant icon for DownloadManagerState. This view have
// fixed size which can not be changed. In "not started" state the icon will be
// an arrow pointing to the ground. In "in progress" state the icon will be a
// small document icon. In "succeeded" state the icon will be a large document
// icon with blue checkmark badge. In "failed" state the icon will be a large
// document icon with red error badge.
@interface LegacyDownloadManagerStateView : UIView

// Changes the icon appropriate for the given state.
@property(nonatomic) DownloadManagerState state;

// Color for download icon in "not started" state.
@property(nonatomic) UIColor* downloadColor;

// Color for document icon in "in progress", "succeeded" and "failed" states.
@property(nonatomic) UIColor* documentColor;

// Allows setting the state with animation.
- (void)setState:(DownloadManagerState)state animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_LEGACY_DOWNLOAD_MANAGER_STATE_VIEW_H_
