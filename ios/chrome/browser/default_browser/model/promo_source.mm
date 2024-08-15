// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/promo_source.h"

#import "base/notreached.h"

std::string_view DefaultBrowserSettingsPageSourceToString(
    DefaultBrowserSettingsPageSource source) {
  switch (source) {
    case DefaultBrowserSettingsPageSource::kSettings:
      return "Settings";
    case DefaultBrowserSettingsPageSource::kOmnibox:
      return "Omnibox";
    case DefaultBrowserSettingsPageSource::kExternalIntent:
      return "ExternalIntent";
    case DefaultBrowserSettingsPageSource::kSetUpList:
      return "SetUpList";
    case DefaultBrowserSettingsPageSource::kExternalAction:
      return "ExternalAction";
    case DefaultBrowserSettingsPageSource::kTipsNotification:
      return "TipsNotification";
  }

  NOTREACHED();
}
