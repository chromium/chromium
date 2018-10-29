// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_
#define IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_

#include <string>

// This file can be empty. Its purpose is to contain the relatively short lived
// declarations required for experimental flags.

namespace experimental_flags {

enum GaiaEnvironment {
  GAIA_ENVIRONMENT_PROD,
  GAIA_ENVIRONMENT_STAGING,
  GAIA_ENVIRONMENT_TEST,
};

enum WhatsNewPromoStatus {
  WHATS_NEW_DEFAULT = 0,         // Not forced to enable a promo.
  WHATS_NEW_TEST_COMMAND_TIP,    // Test Tip that runs a command.
  WHATS_NEW_MOVE_TO_DOCK_TIP,    // Force enable Move To Dock Tip promo.
  WHATS_NEW_PROMO_STATUS_COUNT,  // Count of Whats New Promo Statuses.
};

// Whether the First Run UI will be always be displayed.
bool AlwaysDisplayFirstRun();

GaiaEnvironment GetGaiaEnvironment();

// Returns the host name for an alternative Origin Server host for use by
// |BrandCode| startup ping. Returns empty string if there is no alternative
// host specified.
std::string GetOriginServerHost();

// Returns the promo force enabled, as determined by the experimental flags.
// If |WHATS_NEW_DEFAULT| is returned, no promo is force enabled.
WhatsNewPromoStatus GetWhatsNewPromoStatus();

// Whether memory debugging tools are enabled.
bool IsMemoryDebuggingEnabled();

// Whether startup crash is enabled.
bool IsStartupCrashEnabled();

// Whether the new Clear Browsing Data UI is enabled.
bool IsNewClearBrowsingDataUIEnabled();

// Whether the 3rd party keyboard omnibox workaround is enabled.
bool IsThirdPartyKeyboardWorkaroundEnabled();

// Whether the Bookmarks UI Reboot is enabled.
// TODO (crbug.com/884719): Remove all use of this flag.
bool IsBookmarksUIRebootEnabled();

// Whether the application group sandbox must be cleared before starting.
// Calling this method will reset the flag to false, so the sandbox is cleared
// only once.
bool MustClearApplicationGroupSandbox();

}  // namespace experimental_flags

#endif  // IOS_CHROME_BROWSER_EXPERIMENTAL_FLAGS_H_
