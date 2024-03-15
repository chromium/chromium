// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_PROMO_SOURCE_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_PROMO_SOURCE_H_

#import <string>

// An histogram to report the source of the default browser page.
// Used for UMA, do not reorder.
// LINT.IfChange
enum class DefaultBrowserSettingsPageSource {
  kSettings = 0,
  kOmnibox = 1,
  kExternalIntent = 2,
  kSetUpList = 3,
  // kExternalAction refers to Chrome being opened with a "ChromeExternalAction"
  // host.
  kExternalAction = 4,
  kTipsNotification = 5,
  kMaxValue = kTipsNotification,
};
// When adding new values:
// (1) update ```DefaultBrowserSettingsPageSourceToString``` in
//     /ios/chrome/browser/default_browser/model/promo_source.mm
// (2) update variants for ```IOS.DefaultBrowserSettingsPageUsage{Source}```
//     histogram in // /tools/metrics/histograms/metadata/ios/histograms.xml
// (3) update ```IOSDefaultBrowserSettingsPageSource``` enum in
//     /tools/metrics/histograms/metadata/settings/enums.xml
//
// LINT.ThenChange(/tools/metrics/histograms/metadata/settings/enums.xml)
// and also /ios/chrome/browser/default_browser/model/promo_source.mm
// and also /tools/metrics/histograms/metadata/ios/histograms.xml

// Returns a string representation for enum value. The strings are used as a
// suffix for UMA histograms therefore should match the variants listed in
// "IOS.DefaultBrowserSettingsPageUsage{Source}".
std::string_view DefaultBrowserSettingsPageSourceToString(
    DefaultBrowserSettingsPageSource source);

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_PROMO_SOURCE_H_
