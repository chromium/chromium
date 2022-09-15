// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resources.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "remoting/base/string_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

class ResourcesTest : public testing::Test {
 protected:
  ResourcesTest() : resources_available_(false) {}

  void SetUp() override { resources_available_ = LoadResources("en-US"); }

  void TearDown() override { UnloadResources(); }

  bool resources_available_;
};

// TODO(alexeypa): Reenable the test once http://crbug.com/269143 (ChromeOS) and
// http://crbug.com/268043 (MacOS) are fixed.
TEST_F(ResourcesTest, DISABLED_ProductName) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string expected_product_name = "Chrome Remote Desktop";
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string expected_product_name = "Chromoting";
#endif  // BUILDFLAGdefined(GOOGLE_CRANDING)

  // Chrome-style i18n is not used on Windows or Android.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(resources_available_);
#else
  EXPECT_TRUE(resources_available_);
#endif

  if (resources_available_) {
    EXPECT_EQ(expected_product_name,
              l10n_util::GetStringUTF8(IDS_PRODUCT_NAME));
  }
}

}  // namespace remoting
