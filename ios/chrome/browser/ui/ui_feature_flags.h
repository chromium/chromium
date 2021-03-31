// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_

#include "Availability.h"
#include "base/feature_list.h"

// Feature to open tab switcher after sliding down the toolbar.
extern const base::Feature kExpandedTabStrip;

// Feature to apply UI Refresh theme to the settings.
extern const base::Feature kSettingsRefresh;

// Feature flag to use the unstacked tabstrip when voiceover is enabled.
extern const base::Feature kVoiceOverUnstackedTabstrip;

// Feature flag to always force an unstacked tabstrip.
extern const base::Feature kForceUnstackedTabstrip;

// Test-only: Feature flag used to verify that EG2 can trigger flags. Must be
// always disabled by default, because it is used to verify that enabling
// features in tests works.
extern const base::Feature kTestFeature;

// Feature flag to enable showing a different UI when the setting is managed by
// an enterprise policy.
extern const base::Feature kEnableIOSManagedSettingsUI;

// Feature flag to enable new illustrations and UI on empty states.
extern const base::Feature kIllustratedEmptyStates;

// Feature flag to enable Shared Highlighting (Link to Text). Also enable
// kScrollToTextIOS to successfully open these links.
extern const base::Feature kSharedHighlightingIOS;

// Feature flag that enables taking fullpage screenshots of a webpage.
extern const base::Feature kEnableFullPageScreenshot;

// Feature flag that enables the button in the settings to send the users in the
// Settings.app to update the default browser.
extern const base::Feature kDefaultBrowserSettings;

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished. Flag to modernize the tabstrip without disturbing the existing one.
extern const base::Feature kModernTabStrip;

// Adds a setting to enable biometric authentication for incognito tabs.
extern const base::Feature kIncognitoAuthentication;

// Feature flag that experiments with new location permissions user experiences.
extern const base::Feature kLocationPermissionsPrompt;

// Feature flag that experiments with the default browser fullscreen promo UI.
extern const base::Feature kDefaultBrowserFullscreenPromoExperiment;

// Feature flag that experiments with the default browser fullscreen promo UI.
extern const base::Feature kDefaultBrowserFullscreenPromoCTAExperiment;

// Feature flag that enables non-modal default browser promos.
extern const base::Feature kDefaultPromoNonModal;

// Feature flag that enables tailored fullscreen browser promos.
extern const base::Feature kDefaultPromoTailored;

// Feature flag that swaps the omnibox textfield implementation.
extern const base::Feature kIOSNewOmniboxImplementation;

// Feature flag that enables persisting the Crash Restore Infobar across
// navigations.
extern const base::Feature kIOSPersistCrashRestore;

// Enables the Search History Link in Clear Browsing Data for iOS.
extern const base::Feature kSearchHistoryLinkIOS;

#endif  // IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
