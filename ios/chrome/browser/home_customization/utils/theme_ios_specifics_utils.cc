// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/home_customization/utils/theme_ios_specifics_utils.h"

#include "components/sync/protocol/theme_ios_specifics.pb.h"
#include "components/sync/protocol/theme_types.pb.h"

namespace home_customization {

bool AreNtpCustomBackgroundsEquivalent(const sync_pb::NtpCustomBackground& a,
                                       const sync_pb::NtpCustomBackground& b) {
  return a.url() == b.url();
}

bool AreUserColorThemesEquivalent(const sync_pb::UserColorTheme& a,
                                  const sync_pb::UserColorTheme& b) {
  return a.color() == b.color() &&
         a.browser_color_variant() == b.browser_color_variant();
}

bool AreThemeIosSpecificsEquivalent(const sync_pb::ThemeIosSpecifics& a,
                                    const sync_pb::ThemeIosSpecifics& b) {
  if (a.has_ntp_background() != b.has_ntp_background()) {
    return false;
  }

  if (a.has_ntp_background()) {
    return AreNtpCustomBackgroundsEquivalent(a.ntp_background(),
                                             b.ntp_background());
  }

  if (a.has_user_color_theme() != b.has_user_color_theme()) {
    return false;
  }

  if (a.has_user_color_theme()) {
    return AreUserColorThemesEquivalent(a.user_color_theme(),
                                        b.user_color_theme());
  }

  return true;
}

}  // namespace home_customization
