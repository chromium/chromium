// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FLAGS_CHROME_SWITCHES_H_
#define IOS_CHROME_BROWSER_FLAGS_CHROME_SWITCHES_H_

// -----------------------------------------------------------------------------
// When commenting your switch, please use the same voice as surrounding
// comments. Imagine "This switch..." at the beginning of the phrase, and it'll
// all work out.
// -----------------------------------------------------------------------------

// Defines all the command-line switches used by iOS Chrome.

namespace switches {

// Disable showing available password credentials in the keyboard accessory
// view when focused on form fields.
extern const char kDisableIOSPasswordSuggestions[];

// Disables the 3rd party keyboard omnibox workaround.
extern const char kDisableThirdPartyKeyboardWorkaround[];

// Enables support for Handoff from Chrome on iOS to the default browser of
// other Apple devices.
extern const char kEnableIOSHandoffToOtherDevices[];

// Enables the Spotlight actions.
extern const char kEnableSpotlightActions[];

// Enables the 3rd party keyboard omnibox workaround.
extern const char kEnableThirdPartyKeyboardWorkaround[];

// Enabled the NTP Discover feed.
extern const char kEnableDiscoverFeed[];

// Enables the fullscreen sign-in promo.
extern const char kEnableFullscreenSigninPromo[];

// Enables device switcher experience for the segment specified in the argument,
// e.g. "Android."
extern const char kForceDeviceSwitcherExperienceCommandLineFlag[];

// Enables shopping feature user experience for the segment specified in the
// argument, e.g. "ShoppingUser" or "Other".
extern const char kForceShopperExperience[];

// A string used to override the default user agent with a custom one.
extern const char kUserAgent[];

// Force the discover feed to show the sign-in promo.
extern const char kForceFeedSigninPromo[];
}  // namespace switches

#endif  // IOS_CHROME_BROWSER_FLAGS_CHROME_SWITCHES_H_
