// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/user_agent.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

struct BuildOSCpuInfoTestCases {
  std::string os_version;
  std::string cpu_type;
  std::string expected_os_cpu_info;
};

}  // namespace

TEST(UserAgentStringTest, BuildOSCpuInfoFromOSVersionAndCpuType) {
  const BuildOSCpuInfoTestCases test_cases[] = {
#if BUILDFLAG(IS_WIN)
    // On Windows, it's possible to have an empty string for CPU type.
    {
        /*os_version=*/"10.0",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/"Windows NT 10.0",
    },
    {
        /*os_version=*/"10.0",
        /*cpu_type=*/"WOW64",
        /*expected_os_cpu_info=*/"Windows NT 10.0; WOW64",
    },
    {
        /*os_version=*/"10.0",
        /*cpu_type=*/"Win64; x64",
        /*expected_os_cpu_info=*/"Windows NT 10.0; Win64; x64",
    },
    {
        /*os_version=*/"7.0",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/"Windows NT 7.0",
    },
    // These cases should never happen in real life, but may be useful to detect
    // changes when things are refactored.
    {
        /*os_version=*/"",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/"Windows NT ",
    },
    {
        /*os_version=*/"VERSION",
        /*cpu_type=*/"CPU TYPE",
        /*expected_os_cpu_info=*/"Windows NT VERSION; CPU TYPE",
    },
#elif BUILDFLAG(IS_MAC)
    {
        /*os_version=*/"10_15_4",
        /*cpu_type=*/"Intel",
        /*expected_os_cpu_info=*/"Intel Mac OS X 10_15_4",
    },
    // These cases should never happen in real life, but may be useful to detect
    // changes when things are refactored.
    {
        /*os_version=*/"",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/" Mac OS X ",
    },
    {
        /*os_version=*/"VERSION",
        /*cpu_type=*/"CPU TYPE",
        /*expected_os_cpu_info=*/"CPU TYPE Mac OS X VERSION",
    },
#elif BUILDFLAG(IS_CHROMEOS_ASH)
    {
        /*os_version=*/"4537.56.0",
        /*cpu_type=*/"armv7l",
        /*expected_os_cpu_info=*/"CrOS armv7l 4537.56.0",
    },
    // These cases should never happen in real life, but may be useful to detect
    // changes when things are refactored.
    {
        /*os_version=*/"",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/"CrOS  ",
    },
    {
        /*os_version=*/"VERSION",
        /*cpu_type=*/"CPU TYPE",
        /*expected_os_cpu_info=*/"CrOS CPU TYPE VERSION",
    },
#elif BUILDFLAG(IS_ANDROID)
    {
        /*os_version=*/"7.1.1",
        /*cpu_type=*/"UNUSED",
        /*expected_os_cpu_info=*/"Android 7.1.1",
    },
    // These cases should never happen in real life, but may be useful to detect
    // changes when things are refactored.
    {
        /*os_version=*/"",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/"Android ",
    },
    {
        /*os_version=*/"VERSION",
        /*cpu_type=*/"CPU TYPE",
        /*expected_os_cpu_info=*/"Android VERSION",
    },
#elif BUILDFLAG(IS_FUCHSIA)
    {
        /*os_version=*/"VERSION",
        /*cpu_type=*/"CPU TYPE",
        /*expected_os_cpu_info=*/"Fuchsia",
    },
#endif
  };

  for (const auto& test_case : test_cases) {
    const std::string os_cpu_info = BuildOSCpuInfoFromOSVersionAndCpuType(
        test_case.os_version, test_case.cpu_type);
    EXPECT_EQ(os_cpu_info, test_case.expected_os_cpu_info);
  }
}

TEST(UserAgentStringTest, GetCpuArchitecture) {
  std::string arch = GetCpuArchitecture();

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ("", arch);
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_POSIX)
  EXPECT_TRUE("arm" == arch || "x86" == arch);
#else
#error Unsupported platform
#endif
}

TEST(UserAgentStringTest, GetCpuBitness) {
  std::string bitness = GetCpuBitness();

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ("", bitness);
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_POSIX)
  EXPECT_TRUE("32" == bitness || "64" == bitness);
#else
#error Unsupported platform
#endif
}

}  // namespace content
