// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_USER_AGENT_H_
#define CONTENT_PUBLIC_COMMON_USER_AGENT_H_

#include <string>

#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/user_agent/user_agent_metadata.mojom.h"

namespace content {

namespace frozen_user_agent_strings {

const char kDesktop[] =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
    "like Gecko) Chrome/75.0.3764.0 Safari/537.36";
const char kAndroid[] =
    "Mozilla/5.0 (Linux; Android 9; Unspecified Device) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/75.0.3764.0 "
    "Safari/537.36";
const char kAndroidMobile[] =
    "Mozilla/5.0 (Linux; Android 9; Unspecified Device) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/75.0.3764.0 Mobile "
    "Safari/537.36";

}  // namespace frozen_user_agent_strings

// Returns the WebKit version, in the form "major.minor (branch@revision)".
CONTENT_EXPORT std::string GetWebKitVersion();

CONTENT_EXPORT std::string GetWebKitRevision();

// Builds a User-agent compatible string that describes the OS and CPU type.
// On Android, the string will only include the build number if true is passed
// as an argument.
CONTENT_EXPORT std::string BuildOSCpuInfo(bool include_android_build_number);

// Returns the frozen User-agent string for
// https://github.com/WICG/ua-client-hints.
CONTENT_EXPORT base::StringPiece GetFrozenUserAgent(bool mobile);

// Helper function to generate a full user agent string from a short
// product name.
CONTENT_EXPORT std::string BuildUserAgentFromProduct(
    const std::string& product);

#if defined(OS_ANDROID)
// Helper function to generate a full user agent string given a short
// product name and some extra text to be added to the OS info.
// This is currently only used for Android Web View.
CONTENT_EXPORT std::string BuildUserAgentFromProductAndExtraOSInfo(
    const std::string& product,
    const std::string& extra_os_info,
    bool include_android_build_number);

// Helper function to generate just the OS info.
CONTENT_EXPORT std::string GetAndroidOSInfo(bool include_android_build_number);
#endif

// Builds a full user agent string given a string describing the OS and a
// product name.
CONTENT_EXPORT std::string BuildUserAgentFromOSAndProduct(
    const std::string& os_info,
    const std::string& product);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_USER_AGENT_H_
