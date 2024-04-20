// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_USER_AGENT_H_
#define CONTENT_PUBLIC_COMMON_USER_AGENT_H_

#include <string>

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

enum class IncludeAndroidBuildNumber { Include, Exclude };
enum class IncludeAndroidModel { Include, Exclude };

// Returns the (incorrectly named, for historical reasons) WebKit version, in
// the form "major.minor (@chromium_git_revision)".
CONTENT_EXPORT std::string GetWebKitVersion();

CONTENT_EXPORT std::string GetChromiumGitRevision();

// Builds a string that describes the CPU type when available (or blank
// otherwise).
CONTENT_EXPORT std::string BuildCpuInfo();

// Takes the cpu info (see BuildCpuInfo()) and extracts the architecture for
// most common cases.
CONTENT_EXPORT std::string GetCpuArchitecture();

// Takes the cpu info (see BuildCpuInfo()) and extracts the CPU bitness for
// most common cases.
CONTENT_EXPORT std::string GetCpuBitness();

// Builds a User-agent compatible string that describes the OS and CPU type.
// On Android, the string will only include the build number and model if
// relevant enums indicate they should be included.
CONTENT_EXPORT std::string BuildOSCpuInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model);
// We may also build the same User-agent compatible string describing OS and CPU
// type by providing our own |os_version| and |cpu_type|. This is primarily
// useful in testing.
CONTENT_EXPORT std::string BuildOSCpuInfoFromOSVersionAndCpuType(
    const std::string& os_version,
    const std::string& cpu_type);

// Returns the OS version.
// On Android, the string will only include the build number and model if
// relevant enums indicate they should be included.
CONTENT_EXPORT std::string GetOSVersion(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model);

// Returns the reduced User-agent string for
// https://github.com/WICG/ua-client-hints.
CONTENT_EXPORT std::string GetReducedUserAgent(bool mobile,
                                               std::string major_version);

// TODO(crbug.com/40200617): Remove this after user agent reduction phase 5 and
// --force-major-version-to-minor is removed.
// Return the <unifiedPlatform> token of a reduced User-Agent header.
CONTENT_EXPORT std::string GetUnifiedPlatformForTesting();

// Helper function to generate a full user agent string from a short
// product name.
CONTENT_EXPORT std::string BuildUserAgentFromProduct(
    const std::string& product);

// Helper function to generate a reduced user agent string with unified
// platform from a given product name.
CONTENT_EXPORT std::string BuildUnifiedPlatformUserAgentFromProduct(
    const std::string& product);

// Returns the model information. Returns a blank string if not on Android or
// if on a codenamed (i.e. not a release) build of an Android.
CONTENT_EXPORT std::string BuildModelInfo();

#if BUILDFLAG(IS_ANDROID)
// Helper function to generate a full user agent string given a short
// product name and some extra text to be added to the OS info.
// This is currently only used for Android Web View.
CONTENT_EXPORT std::string BuildUserAgentFromProductAndExtraOSInfo(
    const std::string& product,
    const std::string& extra_os_info,
    IncludeAndroidBuildNumber include_android_build_number);

// Helper function to generate a reduced user agent string with unified
// platform from a given product name and extra os information.
CONTENT_EXPORT std::string BuildUnifiedPlatformUAFromProductAndExtraOs(
    const std::string& product,
    const std::string& extra_os_info);

// Helper function to generate just the OS info.
CONTENT_EXPORT std::string GetAndroidOSInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model);
#endif

// Builds a full user agent string given a string describing the OS and a
// product name.
CONTENT_EXPORT std::string BuildUserAgentFromOSAndProduct(
    const std::string& os_info,
    const std::string& product);

// Returns true if the binary was built in 32-bit mode and is running on 64-bit
// Windows; returns false otherwise.
CONTENT_EXPORT bool IsWoW64();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_USER_AGENT_H_
