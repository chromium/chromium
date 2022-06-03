// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_COORDINATOR_H_

#import "ios/chrome/browser/download/download_manager_tab_helper_delegate.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

namespace web {
class DownloadTask;
}  // namespace web

@protocol ContainedPresenter;

// Coordinates presentation of Download Manager UI.
@interface DownloadManagerCoordinator
    : ChromeCoordinator<DownloadManagerTabHelperDelegate>

// Presents the receiver's view controller.
@property(nonatomic) id<ContainedPresenter> presenter;

// YES if presentation should be animated. Default is NO.
@property(nonatomic) BOOL animatesPresentation;

// Download Manager supports only one download task at a time. Set to null when
// stop method is called.
@property(nonatomic) web::DownloadTask* downloadTask;

// Controls the height of the bottom margin.
@property(nonatomic) NSLayoutDimension* bottomMarginHeightAnchor;

// Underlying UIViewController presented by this coordinator.
@property(nonatomic, readonly) UIViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_COORDINATOR_H_
