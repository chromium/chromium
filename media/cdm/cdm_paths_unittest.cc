// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_paths.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

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
#elif BUILDFLAG(IS_FUCHSIA)
    "fuchsia";
#else
#error unsupported platform
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
#error unsupported architecture
#endif

}  // namespace

TEST(CdmPathsTest, FlagSpecified) {
  EXPECT_FALSE(std::string(BUILDFLAG(CDM_PLATFORM_SPECIFIC_PATH)).empty());
}

TEST(CdmPathsTest, Prefix) {
  const char kPrefix[] = "prefix";
  auto path = GetPlatformSpecificDirectory(kPrefix);

  EXPECT_TRUE(base::StartsWith(path.MaybeAsASCII(), kPrefix,
                               base::CompareCase::SENSITIVE));
}

TEST(CdmPathsTest, Expected) {
  // The same prefix that will be passed to the function under test followed by
  // the special path used by Chrome's component updater.
  std::string expected_path = base::StringPrintf(
      "SomeCdm/_platform_specific/%s_%s", kComponentPlatform, kComponentArch);

#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(base::ReplaceChars(expected_path, "/", "\\", &expected_path));
#endif

  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(expected_path),
            GetPlatformSpecificDirectory("SomeCdm"));
}

}  // namespace media
