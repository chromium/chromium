// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ui_feature_flags.h"

const base::Feature kExpandedTabStrip{"ExpandedTabStrip",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSettingsRefresh{"SettingsRefresh",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kVoiceOverUnstackedTabstrip{
    "VoiceOverUnstackedTabstrip", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kForceUnstackedTabstrip{"ForceUnstackedTabstrip",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTestFeature{"TestFeature",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableIOSManagedSettingsUI{
    "EnableIOSManagedSettingsUI", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kIllustratedEmptyStates{"IllustratedEmptyStates",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSharedHighlightingIOS{"SharedHighlightingIOS",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableFullPageScreenshot{
    "EnableFullPageScreenshot", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDefaultBrowserSettings{"DefaultBrowserSettings",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished.
const base::Feature kModernTabStrip{"ModernTabStrip",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIncognitoAuthentication{
    "enable-incognito-authentication-ios", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLocationPermissionsPrompt{
    "LocationPermissionsPrompt", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDefaultBrowserFullscreenPromoExperiment{
    "DefaultBrowserFullscreenPromoExperiment",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDefaultBrowserFullscreenPromoCTAExperiment{
    "DefaultBrowserFullscreenPromoCTAExperiment",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDefaultPromoNonModal{"DefaultPromoNonModal",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDefaultPromoTailored{"DefaultPromoTailored",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIOSNewOmniboxImplementation{
    "kIOSNewOmniboxImplementation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIOSPersistCrashRestore{"IOSPersistCrashRestore",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSearchHistoryLinkIOS{"SearchHistoryLinkIOS",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
