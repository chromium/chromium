// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_MANAGER_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_MANAGER_VIEW_CONTROLLER_TESTING_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/download/ui/download_manager_view_controller.h"

// Interface to expose `DownloadManagerViewController` UI elements for testing
// purposes.
@interface DownloadManagerViewController (Testing)

// The currently presented action button ("Download", "Open in...", etc).
@property(nonatomic, readonly) UIButton* actionButton;

// The download manager status label i.e. main label.
@property(nonatomic, readonly) UILabel* statusLabel;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_MANAGER_VIEW_CONTROLLER_TESTING_H_
