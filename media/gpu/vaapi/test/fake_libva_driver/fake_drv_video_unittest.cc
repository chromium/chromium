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

TEST_F(FakeDriverTest, CanCreateConfig) {
  VAConfigID config_id = VA_INVALID_ID;
  const VAStatus va_res =
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr,
                     /*num_attribs=*/0, &config_id);
  ASSERT_EQ(VA_STATUS_SUCCESS, va_res);
  EXPECT_NE(VA_INVALID_ID, config_id);
}

TEST_F(FakeDriverTest, QueryConfigAttributesForValidConfigID) {
  VAConfigID config_id;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  VAProfile profile = VAProfileNone;
  VAEntrypoint entrypoint = VAEntrypointProtectedContent;
  VAConfigAttrib config_attribs[base::checked_cast<size_t>(
      vaMaxNumConfigAttributes(display_))];
  memset(config_attribs, 0, sizeof(config_attribs));
  int num_attribs = 0;
  const VAStatus va_res = vaQueryConfigAttributes(
      display_, config_id, &profile, &entrypoint, config_attribs, &num_attribs);
  ASSERT_EQ(VA_STATUS_SUCCESS, va_res);
  EXPECT_EQ(VAProfileVP8Version0_3, profile);
  EXPECT_EQ(VAEntrypointVLD, entrypoint);
  EXPECT_NE(0, num_attribs);
}

TEST_F(FakeDriverTest, QueryConfigAttributesCrashesForInvalidConfigID) {
  VAProfile profile;
  VAEntrypoint entrypoint;
  VAConfigAttrib config_attribs[base::checked_cast<size_t>(
      vaMaxNumConfigAttributes(display_))];
  memset(config_attribs, 0, sizeof(config_attribs));
  int num_attribs;
  EXPECT_DEATH(
      vaQueryConfigAttributes(display_, /*config_id=*/0, &profile, &entrypoint,
                              config_attribs, &num_attribs);
      , "");
}

TEST_F(FakeDriverTest, CreateContextForValidConfigID) {
  VAConfigID config_id;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  VAContextID context_id = VA_INVALID_ID;
  const VAStatus va_res = vaCreateContext(
      display_, config_id, /*picture_width=*/1280, /*picture_height=*/720,
      /*flag=*/0, /*render_targets=*/nullptr,
      /*num_render_targets=*/0, &context_id);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
}

TEST_F(FakeDriverTest, CreateContextCrashesForInvalidConfigID) {
  VAContextID context_id;
  EXPECT_DEATH(
      vaCreateContext(display_, /*config_id=*/0, /*picture_width=*/1280,
                      /*picture_height=*/720, /*flag=*/0,
                      /*render_targets=*/nullptr,
                      /*num_render_targets=*/0, &context_id),
      "");
}

TEST_F(FakeDriverTest, QuerySurfaceAttributesForValidConfigID) {
  VAConfigID config_id;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  VASurfaceAttrib surface_attribs[32];
  unsigned int num_attribs = 32;
  memset(surface_attribs, 0, sizeof(surface_attribs));

  const VAStatus va_res = vaQuerySurfaceAttributes(
      display_, config_id, surface_attribs, &num_attribs);
  ASSERT_EQ(VA_STATUS_SUCCESS, va_res);
  EXPECT_NE(0u, num_attribs);
}

TEST_F(FakeDriverTest, QuerySurfaceAttributesCrashesForInvalidConfigID) {
  VASurfaceAttrib surface_attribs[32];
  unsigned int num_attribs = 32;
  memset(surface_attribs, 0, sizeof(surface_attribs));

  EXPECT_DEATH(vaQuerySurfaceAttributes(display_, /*config_id=*/0,
                                        surface_attribs, &num_attribs),
               "");
}

TEST_F(FakeDriverTest, DestroyConfigForValidConfigID) {
  VAConfigID config_id;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  const VAStatus va_res = vaDestroyConfig(display_, config_id);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
}

TEST_F(FakeDriverTest, DestroyConfigCrashesForInvalidConfigID) {
  EXPECT_DEATH(vaDestroyConfig(display_, /*config_id=*/0), "");
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
