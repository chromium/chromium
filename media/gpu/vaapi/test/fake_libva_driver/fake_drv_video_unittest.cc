// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <va/va.h>
#include <va/va_drm.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "media/gpu/vaapi/va_stubs.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
using media_gpu_vaapi::kModuleVa_prot;
#endif

using media_gpu_vaapi::kModuleVa;
using media_gpu_vaapi::kModuleVa_drm;
using media_gpu_vaapi::StubPathMap;

class FakeDriverTest : public testing::Test {
 public:
  FakeDriverTest() = default;
  FakeDriverTest(const FakeDriverTest&) = delete;
  FakeDriverTest& operator=(const FakeDriverTest&) = delete;
  ~FakeDriverTest() override = default;

  void SetUp() override {
    display_ = vaGetDisplayDRM(0);
    ASSERT_TRUE(vaDisplayIsValid(display_));
    int major_version;
    int minor_version;

    ASSERT_EQ(vaInitialize(display_, &major_version, &minor_version),
              VA_STATUS_SUCCESS);
  }

  void TearDown() override {
    ASSERT_EQ(vaTerminate(display_), VA_STATUS_SUCCESS);
  }

 protected:
  VADisplay display_ = nullptr;
};

TEST_F(FakeDriverTest, VerifyQueryConfigProfiles) {
  ASSERT_GT(vaMaxNumProfiles(display_), 0);

  std::vector<VAProfile> va_profiles(
      base::checked_cast<size_t>(vaMaxNumProfiles(display_)));
  int num_va_profiles;

  const VAStatus va_res =
      vaQueryConfigProfiles(display_, va_profiles.data(), &num_va_profiles);
  EXPECT_EQ(va_res, VA_STATUS_SUCCESS);

  std::set<VAProfile> unique_profiles(va_profiles.begin(), va_profiles.end());
  EXPECT_EQ(va_profiles.size(), unique_profiles.size());
}

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  const std::string va_suffix(base::NumberToString(VA_MAJOR_VERSION + 1));
  StubPathMap paths;

  paths[kModuleVa].push_back(std::string("libva.so.") + va_suffix);
  paths[kModuleVa_drm].push_back(std::string("libva-drm.so.") + va_suffix);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  paths[kModuleVa_prot].push_back(std::string("libva.so.") + va_suffix);
#endif

  // InitializeStubs dlopen() VA-API libraries.
  const bool result = InitializeStubs(paths);
  if (!result) {
    LOG(ERROR) << "Failed to initialize the libva symbols";
    return EXIT_FAILURE;
  }

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
