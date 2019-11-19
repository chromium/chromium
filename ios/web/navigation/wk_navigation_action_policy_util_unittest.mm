// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_action_policy_util.h"

#import <WebKit/WebKit.h>

#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#include "ios/web/navigation/block_universal_links_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

using WKNavigationActionPolicyUtilTest = PlatformTest;

// Tests GetAllowNavigationActionPolicy for normal browsing mode.
TEST_F(WKNavigationActionPolicyUtilTest, AllowNavigationActionPolicy) {
  WKNavigationActionPolicy policy = GetAllowNavigationActionPolicy(false);
  EXPECT_EQ(WKNavigationActionPolicyAllow, policy);
}

// Tests GetAllowNavigationActionPolicy for off the record browsing mode with
// the |kBlockUniversalLinksInOffTheRecordMode| feature disabled.
TEST_F(WKNavigationActionPolicyUtilTest,
       AllowNavigationActionPolicyForOffTheRecord) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      web::features::kBlockUniversalLinksInOffTheRecordMode);

  WKNavigationActionPolicy policy = GetAllowNavigationActionPolicy(true);
  EXPECT_EQ(WKNavigationActionPolicyAllow, policy);
}

// Tests GetAllowNavigationActionPolicy for off the record browsing mode with
// the |kBlockUniversalLinksInOffTheRecordMode| feature enabled.
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
