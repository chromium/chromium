// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_paths.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

// Only verify platform specific path on some platforms.
// TODO(crbug.com/971433). Move the CDMs out of the install directory on
// ChromeOS.
#if (defined(OS_MACOSX) || defined(OS_WIN) ||         \
     (defined(OS_LINUX) && !defined(OS_CHROMEOS))) && \
    (defined(ARCH_CPU_X86) || defined(ARCH_CPU_X86_64))
#define CDM_USE_PLATFORM_SPECIFIC_PATH
#endif

namespace media {

namespace {

#if defined(CDM_USE_PLATFORM_SPECIFIC_PATH)

// Special path used in chrome components.
const char kPlatformSpecific[] = "_platform_specific";

// Name of the component platform.
const char kComponentPlatform[] =
#if defined(OS_MACOSX)
    "mac";
#elif defined(OS_WIN)
    "win";
#elif defined(OS_CHROMEOS)
    "cros";
#elif defined(OS_LINUX)
    "linux";
#else
    "unsupported_platform";
#endif

// Name of the component architecture.
const char kComponentArch[] =
#if defined(ARCH_CPU_X86)
    "x86";
#elif defined(ARCH_CPU_X86_64)
    "x64";
#elif defined(ARCH_CPU_ARMEL)
    "arm";
#else
    "unsupported_arch";
#endif

base::FilePath GetExpectedPlatformSpecificDirectory(
    const std::string& base_path) {
  base::FilePath path;
  const std::string kPlatformArch =
      std::string(kComponentPlatform) + "_" + kComponentArch;
  return path.AppendASCII(base_path)
      .AppendASCII(kPlatformSpecific)
      .AppendASCII(kPlatformArch);
}

#else

// If the CDM is not a component, it has no platform specific path.
base::FilePath GetExpectedPlatformSpecificDirectory(
    const std::string& base_path) {
  return base::FilePath();
}

#endif  // defined(CDM_USE_PLATFORM_SPECIFIC_PATH)

std::string GetFlag() {
  return BUILDFLAG(CDM_PLATFORM_SPECIFIC_PATH);
}

}  // namespace

TEST(CdmPathsTest, FlagSpecified) {
#if defined(CDM_USE_PLATFORM_SPECIFIC_PATH)
  EXPECT_FALSE(GetFlag().empty());
#else
  EXPECT_TRUE(GetFlag().empty());
#endif
}

TEST(CdmPathsTest, Prefix) {
  const char kPrefix[] = "prefix";
  auto path = GetPlatformSpecificDirectory(kPrefix);

#if defined(CDM_USE_PLATFORM_SPECIFIC_PATH)
  EXPECT_TRUE(base::StartsWith(path.MaybeAsASCII(), kPrefix,
                               base::CompareCase::SENSITIVE));
#else
  EXPECT_TRUE(path.MaybeAsASCII().empty());
#endif
}

TEST(CdmPathsTest, Expected) {
  const char kPrefix[] = "cdm";
  EXPECT_EQ(GetExpectedPlatformSpecificDirectory(kPrefix),
            GetPlatformSpecificDirectory(kPrefix));
}

}  // namespace media
