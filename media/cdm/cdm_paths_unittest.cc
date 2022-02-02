// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_paths.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Special path used in chrome components.
const char kPlatformSpecific[] = "_platform_specific";

// Name of the component platform.
const char kComponentPlatform[] =
#if BUILDFLAG(IS_MAC)
    "mac";
#elif BUILDFLAG(IS_WIN)
    "win";
#elif BUILDFLAG(IS_CHROMEOS)
    "cros";
#elif BUILDFLAG(IS_LINUX)
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
#elif defined(ARCH_CPU_ARM64)
    "arm64";
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

std::string GetFlag() {
  return BUILDFLAG(CDM_PLATFORM_SPECIFIC_PATH);
}

}  // namespace

TEST(CdmPathsTest, FlagSpecified) {
  EXPECT_FALSE(GetFlag().empty());
}

TEST(CdmPathsTest, Prefix) {
  const char kPrefix[] = "prefix";
  auto path = GetPlatformSpecificDirectory(kPrefix);

  EXPECT_TRUE(base::StartsWith(path.MaybeAsASCII(), kPrefix,
                               base::CompareCase::SENSITIVE));
}

TEST(CdmPathsTest, Expected) {
  const char kPrefix[] = "cdm";
  EXPECT_EQ(GetExpectedPlatformSpecificDirectory(kPrefix),
            GetPlatformSpecificDirectory(kPrefix));
}

}  // namespace media
