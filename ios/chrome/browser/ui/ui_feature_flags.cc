// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/ui_feature_flags.h"

const base::Feature kExpandedTabStrip{"ExpandedTabStrip",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTestFeature{"TestFeature",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSharedHighlightingIOS{"SharedHighlightingIOS",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableFREDefaultBrowserPromoScreen{
    "EnableFREDefaultBrowserPromoScreen", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableFREUIModuleIOS{"EnableFREUIModuleIOSV3",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished.
const base::Feature kModernTabStrip{"ModernTabStrip",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIncognitoNtpRevamp{"IncognitoNtpRevamp",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIOS3PIntentsInIncognito{"IOS3pIntentsInIncognito",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDefaultBrowserFullscreenPromoExperiment{
    "DefaultBrowserFullscreenPromoExperiment",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDefaultBrowserIntentsShowSettings{
    "DefaultBrowserIntentsShowSettings", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kIOSNewOmniboxImplementation{
    "kIOSNewOmniboxImplementation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIOSOmniboxUpdatedPopupUI{
    "IOSOmniboxUpdatedPopupUI", base::FEATURE_DISABLED_BY_DEFAULT};

const char kIOSOmniboxUpdatedPopupUIVariationName[] = "PopupUIVariant";

extern const char kIOSOmniboxUpdatedPopupUIVariation1[] = "variant-one";
extern const char kIOSOmniboxUpdatedPopupUIVariation2[] = "variant-two";
extern const char kIOSOmniboxUpdatedPopupUIVariation1UIKit[] =
    "variant-one-UIKit";
extern const char kIOSOmniboxUpdatedPopupUIVariation2UIKit[] =
    "variant-two-UIKit";

const base::Feature kIOSLocationBarUseNativeContextMenu{
    "IOSLocationBarUseNativeContextMenu", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUpdateHistoryEntryPointsInIncognito{
    "UpdateHistoryEntryPointsInIncognito", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUseLensToSearchForImage{"UseLensToSearchForImage",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRemoveExcessNTPs{"RemoveExcessNTPs",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableShortenedPasswordAutoFillInstruction{
    "EnableShortenedPasswordAutoFillInstruction",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAddSettingForDefaultPageMode{
    "DefaultRequestedMode", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseSFSymbols{"UseSFSymbols",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCalendarExperienceKit{"CalendarExperienceKit",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableExpKitAppleCalendar{
    "EnableExpKitAppleCalendar", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnablePhoneNumbers{"EnablePhoneNumbers",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMapsExperienceKit{"MapsExperienceKit",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableMiniMap{"EnableMiniMap",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
