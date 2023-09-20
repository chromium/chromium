// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_COORDINATOR_DELEGATE_H_

@class DownloadsSettingsCoordinator;

@protocol DownloadsSettingsCoordinatorDelegate <NSObject>

// Called when the Downloads settings coordinator wants to stop.
- (void)downloadsSettingsCoordinatorWasRemoved:
    (DownloadsSettingsCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_DOWNLOADS_SETTINGS_COORDINATOR_DELEGATE_H_
