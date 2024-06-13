// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_VIEW_CONTROLLER_PROTOCOL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_VIEW_CONTROLLER_PROTOCOL_H_

#import <UIKit/UIKit.h>

@protocol DownloadManagerViewControllerDelegate;
class FullscreenController;
@class LayoutGuideCenter;

// Base protocol for the DownloadManagerViewController.
@protocol DownloadManagerViewControllerProtocol <NSObject>

@property(nonatomic, weak) id<DownloadManagerViewControllerDelegate> delegate;

// The layout guide center to use to retrieve the bottom margin.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Whether the download prompt is displaying in Incognito mode.
@property(nonatomic, assign) BOOL incognito;

// View to use as source for the "Open in" popover.
@property(nonatomic, readonly) UIView* openInSourceView;

// Sets the fullscreen controller to update UI on fullscreen changes.
- (void)setFullscreenController:(FullscreenController*)fullscreenController;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_VIEW_CONTROLLER_PROTOCOL_H_
