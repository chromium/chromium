// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/flags/chrome_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// When commenting your switch, please use the same voice as surrounding
// comments. Imagine "This switch..." at the beginning of the phrase, and it'll
// all work out.
// -----------------------------------------------------------------------------

// Disable showing available password credentials in the keyboard accessory
// view when focused on form fields.
const char kDisableIOSPasswordSuggestions[] =
    "disable-ios-password-suggestions";

// Disables the 3rd party keyboard omnibox workaround.
const char kDisableThirdPartyKeyboardWorkaround[] =
    "disable-third-party-keyboard-workaround";

// Enables the Promo Manager to display full-screen promos on app startup.
const char kEnablePromoManagerFullscreenPromos[] =
    "enable-promo-manager-fullscreen-promos";

// Enables support for Handoff from Chrome on iOS to the default browser of
// other Apple devices.
const char kEnableIOSHandoffToOtherDevices[] =
    "enable-ios-handoff-to-other-devices";

// Enables the Spotlight actions.
const char kEnableSpotlightActions[] = "enable-spotlight-actions";

// Enables the 3rd party keyboard omnibox workaround.
const char kEnableThirdPartyKeyboardWorkaround[] =
    "enable-third-party-keyboard-workaround";

// Enabled the NTP Discover feed.
const char kEnableDiscoverFeed[] = "enable-discover-feed";

// Enables the upgrade sign-in promo.
const char kEnableUpgradeSigninPromo[] = "enable-upgrade-signin-promo";

// Enables device switcher experience for the segment specified in the argument,
// e.g. "Android."
const char kForceDeviceSwitcherExperienceCommandLineFlag[] =
    "force-device-switcher-experience";

// Enables shopping feature user experience for the segment specified in the
// argument, e.g. "ShoppingUser" or "Other".
const char kForceShopperExperience[] = "force-shopper-experience";

// A string used to override the default user agent with a custom one.
const char kUserAgent[] = "user-agent";

}  // namespace switches
