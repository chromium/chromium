// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SYNC_SETTINGS_VIEW_STATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SYNC_SETTINGS_VIEW_STATE_H_

// Protocol used to retrieve view properties in Sync settings UI.
@protocol SyncSettingsViewState

// Whether the Sync settings view is displayed. This does not necessarily mean
// the view is visible to the user since it can be obstructed by views that are
// not owned by the navigation stack (e.g. MyGoogle UI).
@property(nonatomic, assign, readonly, getter=isSettingsViewShown)
    BOOL settingsViewShown;

// UINavigationItem associated with the Sync settings view.
@property(nonatomic, strong, readonly) UINavigationItem* navigationItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SYNC_SETTINGS_VIEW_STATE_H_
