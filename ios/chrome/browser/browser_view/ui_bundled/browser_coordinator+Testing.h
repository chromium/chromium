// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_COORDINATOR_TESTING_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_COORDINATOR_TESTING_H_

#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator.h"

#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_coordinator.h"

@class NewTabPageCoordinator;

// Coordinator for BrowserViewController. Testing category to expose properties
// used for tests only.
@interface BrowserCoordinator (Testing) <ActivityServiceCommands>

// The coordinator used for the New Tab Page.
@property(nonatomic, strong, readonly) NewTabPageCoordinator* NTPCoordinator;

// Coordinator for displaying the Save to Photos UI.
@property(nonatomic, strong, readonly) SaveToPhotosCoordinator* saveToPhotosCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_COORDINATOR_TESTING_H_
