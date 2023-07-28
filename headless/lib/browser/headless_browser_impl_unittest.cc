// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/embedder_support/switches.h"
#include "components/embedder_support/user_agent_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace headless {

TEST(HeadlessBrowserTest, GetUserAgentMetadata) {
  auto metadata = HeadlessBrowser::GetUserAgentMetadata();

  // Most fields should match those from embedder_support.
  auto from_embedder = embedder_support::GetUserAgentMetadata();
  EXPECT_EQ(metadata.architecture, from_embedder.architecture);
  EXPECT_EQ(metadata.bitness, from_embedder.bitness);
  EXPECT_EQ(metadata.full_version, from_embedder.full_version);
  EXPECT_EQ(metadata.mobile, from_embedder.mobile);
  EXPECT_EQ(metadata.model, from_embedder.model);
  EXPECT_EQ(metadata.platform, from_embedder.platform);
  EXPECT_EQ(metadata.platform_version, from_embedder.platform_version);
  EXPECT_EQ(metadata.wow64, from_embedder.wow64);

  // The brand version lists should contain HeadlessChrome.
  for (auto& list :
       {metadata.brand_version_list, metadata.brand_full_version_list}) {
    EXPECT_THAT(list, testing::Contains(
                          testing::Field(&blink::UserAgentBrandVersion::brand,
                                         testing::Eq("HeadlessChrome"))));
  }
}

TEST(HeadlessBrowserTest, CustomUserAgent) {
  std::string custom_user_agent = "custom chrome user agent";
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(embedder_support::kUserAgent,
                                  custom_user_agent);
  ASSERT_TRUE(command_line->HasSwitch(embedder_support::kUserAgent));

  base::test::ScopedFeatureList scoped_feature_list;
  {
    // Verify low-entropy client hints aren't empty.
    auto metadata = HeadlessBrowser::GetUserAgentMetadata();
    EXPECT_THAT(
        metadata.brand_version_list,
        testing::Contains(testing::Field(&blink::UserAgentBrandVersion::brand,
                                         testing::Eq("HeadlessChrome"))));
    EXPECT_NE("", metadata.platform);

    // Verify high-entropy client hints are empty, take platform version as
    // an example to verify.
    EXPECT_EQ("", metadata.platform_version);
  }

  scoped_feature_list.InitAndEnableFeature(blink::features::kUACHOverrideBlank);
  {
    // Make sure return blank values for GetUserAgentMetadata().
    EXPECT_EQ(blink::UserAgentMetadata(),
              HeadlessBrowser::GetUserAgentMetadata());
  }
}

}  // namespace headless
