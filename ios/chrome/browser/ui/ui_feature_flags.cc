// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ui_feature_flags.h"

const base::Feature kBrowserContainerKeepsContentView{
    "BrowserContainerKeepsContentView", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOmniboxPopupShortcutIconsInZeroState{
    "OmniboxPopupShortcutIconsInZeroState", base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(crbug.com/945811): Using |-drawViewHierarchyInRect:afterScreenUpdates:|
// has adverse flickering when taking a snapshot of the NTP while in the app
// switcher.
const base::Feature kSnapshotDrawView{"SnapshotDrawView",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSettingsRefresh{"SettingsRefresh",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNewOmniboxPopupLayout{"NewOmniboxPopupLayout",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOmniboxUseDefaultSearchEngineFavicon{
    "OmniboxUseDefaultSearchEngineFavicon", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLanguageSettings{"LanguageSettings",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEmbedderBlockRestoreUrl{"EmbedderBlockRestoreUrl",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDisableAnimationOnLowBattery{
    "DisableAnimationOnLowBattery", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kVoiceOverUnstackedTabstrip{
    "VoiceOverUnstackedTabstrip", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kForceUnstackedTabstrip{"ForceUnstackedTabstrip",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
