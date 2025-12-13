// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_COMMON_MANIFEST_ID_CONSTANTS_H_
#define COMPONENTS_WEBAPPS_COMMON_MANIFEST_ID_CONSTANTS_H_

#include <string_view>

namespace webapps {

// Google Chat, hosted on mail.google.com:
inline constexpr std::string_view kMailGoogleChatManifestId = {
    "https://mail.google.com/chat/"};
inline constexpr std::string_view kMailGoogleChatInstallUrl = {
    "https://mail.google.com/chat/download"};
// Note: The `AppId` is "mdpkiolbdkhdjpekfbkbmhigcaggjagi", generated via:
// `web_app::GenerateAppIdFromManifestId(GURL(kMailGoogleChatManifestId));`

// Google Chat, hosted on chat.google.com:
inline constexpr std::string_view kGoogleChatManifestId = {
    "https://chat.google.com/"};
// Note: The `AppId` is "pommaclcbfghclhalboakcipcmmndhcj", generated via:
// `web_app::GenerateAppIdFromManifestId(GURL(kGoogleChatManifestId));`

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_COMMON_MANIFEST_ID_CONSTANTS_H_
