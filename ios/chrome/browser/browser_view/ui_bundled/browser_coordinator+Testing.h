// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_COORDINATOR_TESTING_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_COORDINATOR_TESTING_H_

#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"
#import "ios/chrome/browser/save_to_photos/ui_bundled/save_to_photos_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/sync_presenter_commands.h"

@class DownloadListCoordinator;
@class NewTabPageCoordinator;

// Coordinator for BrowserViewController. Testing category to expose properties
// used for tests only.
@interface BrowserCoordinator (Testing) <ActivityServiceCommands,
                                         OverscrollActionsControllerDelegate,
                                         SyncPresenterCommands>

// The coordinator used for the New Tab Page.
@property(nonatomic, strong, readonly) NewTabPageCoordinator* NTPCoordinator;

// Coordinator for displaying the Save to Photos UI.
@property(nonatomic, strong, readonly)
    SaveToPhotosCoordinator* saveToPhotosCoordinator;

// Coordinator for displaying the Download List UI.
@property(nonatomic, strong, readonly)
    DownloadListCoordinator* downloadListCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_COORDINATOR_TESTING_H_
