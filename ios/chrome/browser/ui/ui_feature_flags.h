// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Feature to retain the contentView in the browser container.
extern const base::Feature kBrowserContainerKeepsContentView;

// Feature to show most visited sites and collection shortcuts in the omnibox
// popup instead of ZeroSuggest.
extern const base::Feature kOmniboxPopupShortcutIconsInZeroState;

// Feature to take snapshots using |-drawViewHierarchy:|.
extern const base::Feature kSnapshotDrawView;

// Feature to apply UI Refresh theme to the settings.
extern const base::Feature kSettingsRefresh;

// Feature to display the new omnibox popup design with favicons, search engine
// favicon in the omnibox, rich entities support, new layout.
extern const base::Feature kNewOmniboxPopupLayout;

// Feature to display the omnibox with default search engine favicon
// in the omnibox.
extern const base::Feature kOmniboxUseDefaultSearchEngineFavicon;

// Feature flag for the language settings page.
extern const base::Feature kLanguageSettings;

// Feature flag for embedders to block restore urls.
extern const base::Feature kEmbedderBlockRestoreUrl;

// Feature flag disabling animation on low battery.
extern const base::Feature kDisableAnimationOnLowBattery;

// Feature flag to use the unstacked tabstrip when voiceover is enabled.
extern const base::Feature kVoiceOverUnstackedTabstrip;

// Feature flag to always force an unstacked tabstrip.
extern const base::Feature kForceUnstackedTabstrip;

#endif  // IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
