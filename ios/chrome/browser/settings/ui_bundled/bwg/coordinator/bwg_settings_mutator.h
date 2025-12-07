// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_BWG_SETTINGS_MUTATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_BWG_SETTINGS_MUTATOR_H_

class GURL;

// Mutator protocol for the view controller to communicate with the
// `BWGSettingsMediator`.
@protocol BWGSettingsMutator

// Handles tap on learn about your choices.
- (void)openNewTabWithURL:(const GURL&)URL;

// Sets the Precise Location pref value.
- (void)setPreciseLocationPref:(BOOL)value;

// Sets the Page Content Sharing pref value.
- (void)setPageContentSharingPref:(BOOL)value;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_BWG_SETTINGS_MUTATOR_H_
