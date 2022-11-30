// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_HTTPS_UPGRADE_TYPE_H_
#define IOS_WEB_PUBLIC_NAVIGATION_HTTPS_UPGRADE_TYPE_H_

#include <string>

namespace web {

// Used to specify the type of the HTTPS upgrade that was applied on a
// navigation, if any. Presently, two features can upgrade HTTP navigations to
// HTTPS:
// - HTTPS-Only Mode (aka HTTPS-First Mode)
// - Omnibox navigation upgrades
//
// In both cases, the relevant tab helpers need to know whether the upgraded
// HTTPS navigation failed. They also need to know why the navigation was
// upgraded in the first place so that they can take the appropriate action to
// handle the failure case (e.g. by showing an interstitial in HTTPS-Only Mode
// and by falling back to HTTP in omnibox navigation upgrades).
enum class HttpsUpgradeType {
  // The navigation was not upgraded to HTTPS.
  kNone,

  // Navigation was upgraded to HTTPS by the HTTPS-Only Mode feature.
  kHttpsOnlyMode,

  // Navigation was upgraded to HTTPS by the omnibox.
  kOmnibox,
};

// Returns a string representation of `type`.
std::string GetHttpsUpgradeTypeDescription(HttpsUpgradeType type);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_HTTPS_UPGRADE_TYPE_H_