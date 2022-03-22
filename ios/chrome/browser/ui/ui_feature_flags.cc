// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ui_feature_flags.h"

const base::Feature kExpandedTabStrip{"ExpandedTabStrip",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTestFeature{"TestFeature",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharedHighlightingIOS{"SharedHighlightingIOS",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableFREDefaultBrowserScreenTesting{
    "EnableFREDefaultBrowserScreenTesting", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableFREUIModuleIOS{"EnableFREUIModuleIOSV3",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished.
const base::Feature kModernTabStrip{"ModernTabStrip",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIncognitoBrandConsistencyForIOS{
    "IncognitoBrandConsistencyForIOS", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIncognitoNtpRevamp{"IncognitoNtpRevamp",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDefaultBrowserFullscreenPromoExperiment{
    "DefaultBrowserFullscreenPromoExperiment",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIOSNewOmniboxImplementation{
    "kIOSNewOmniboxImplementation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIOSOmniboxAllowEditsDuringDictation{
    "IOSOmniboxAllowEditsDuringDictation", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kIOSOmniboxUpdatedPopupUI{
    "kIOSOmniboxUpdatedPopupUI", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIOSLocationBarUseNativeContextMenu{
    "IOSLocationBarUseNativeContextMenu", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSearchHistoryLinkIOS{"SearchHistoryLinkIOS",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUpdateHistoryEntryPointsInIncognito{
    "UpdateHistoryEntryPointsInIncognito", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseLensToSearchForImage{"UseLensToSearchForImage",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCredentialProviderExtensionPromo{
    "CredentialProviderExtensionPromo", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRemoveExcessNTPs{"RemoveExcessNTPs",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableShortenedPasswordAutoFillInstruction{
    "EnableShortenedPasswordAutoFillInstruction",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAddSettingForDefaultPageMode{
    "DefaultRequestedMode", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseSFSymbolsSamples{"UseSFSymbolsSamples",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseUIKitPopupMenu{"UseUIKitPopupMenu",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldUseUIKitPopupMenu() {
  return base::FeatureList::IsEnabled(kUseUIKitPopupMenu);
}
