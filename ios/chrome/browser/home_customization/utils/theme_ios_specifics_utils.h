// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_THEME_IOS_SPECIFICS_UTILS_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_THEME_IOS_SPECIFICS_UTILS_H_

namespace sync_pb {
class NtpCustomBackground;
class UserColorTheme;
class ThemeIosSpecifics;
}  // namespace sync_pb

namespace home_customization {

// Comparison helpers for `ThemeIosSpecifics` protos.

// Returns `true` if two `NtpCustomBackground`s are considered equivalent.
bool AreNtpCustomBackgroundsEquivalent(const sync_pb::NtpCustomBackground& a,
                                       const sync_pb::NtpCustomBackground& b);

// Returns `true` if two `UserColorTheme`s are considered equivalent.
bool AreUserColorThemesEquivalent(const sync_pb::UserColorTheme& a,
                                  const sync_pb::UserColorTheme& b);

// Returns `true` if two `ThemeIosSpecifics`s are considered equivalent.
bool AreThemeIosSpecificsEquivalent(const sync_pb::ThemeIosSpecifics& a,
                                    const sync_pb::ThemeIosSpecifics& b);

}  // namespace home_customization

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_THEME_IOS_SPECIFICS_UTILS_H_
