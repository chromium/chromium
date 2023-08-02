// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_action_policy_util.h"

#import <WebKit/WebKit.h>

#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/block_universal_links_buildflags.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace web {

using WKNavigationActionPolicyUtilTest = PlatformTest;

// Tests GetAllowNavigationActionPolicy for normal browsing mode.
TEST_F(WKNavigationActionPolicyUtilTest, AllowNavigationActionPolicy) {
  WKNavigationActionPolicy policy = GetAllowNavigationActionPolicy(false);
  EXPECT_EQ(WKNavigationActionPolicyAllow, policy);
}

// Tests GetAllowNavigationActionPolicy for off the record browsing mode with
// the `kBlockUniversalLinksInOffTheRecordMode` feature disabled.
TEST_F(WKNavigationActionPolicyUtilTest,
       AllowNavigationActionPolicyForOffTheRecord) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      web::features::kBlockUniversalLinksInOffTheRecordMode);

  WKNavigationActionPolicy policy = GetAllowNavigationActionPolicy(true);
  EXPECT_EQ(WKNavigationActionPolicyAllow, policy);
}

// Tests GetAllowNavigationActionPolicy for off the record browsing mode with
// the `kBlockUniversalLinksInOffTheRecordMode` feature enabled.
TEST_F(WKNavigationActionPolicyUtilTest, BlockUniversalLinksForOffTheRecord) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      web::features::kBlockUniversalLinksInOffTheRecordMode);

  WKNavigationActionPolicy expected_policy = WKNavigationActionPolicyAllow;
#if BUILDFLAG(BLOCK_UNIVERSAL_LINKS_IN_OFF_THE_RECORD_MODE)
  expected_policy = kNavigationActionPolicyAllowAndBlockUniversalLinks;
#endif  // BUILDFLAG(BLOCK_UNIVERSAL_LINKS_IN_OFF_THE_RECORD_MODE)

  EXPECT_EQ(expected_policy, GetAllowNavigationActionPolicy(true));
}

}  // namespace web
