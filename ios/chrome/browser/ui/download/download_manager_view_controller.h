// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/download/download_manager_consumer.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller_protocol.h"

// Presents revamped bottom bar UI for a single download task.
@interface DownloadManagerViewController
    : UIViewController <DownloadManagerConsumer,
                        DownloadManagerViewControllerProtocol>

// DownloadManagerViewControllerProtocol overrides.
@property(nonatomic, weak) id<DownloadManagerViewControllerDelegate> delegate;
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
@property(nonatomic, assign) BOOL incognito;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_
