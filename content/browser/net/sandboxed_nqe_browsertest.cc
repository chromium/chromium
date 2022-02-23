// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "sandbox/policy/features.h"

namespace content {
namespace {

class SandboxedNQEBrowserTest : public ContentBrowserTest {
 public:
  SandboxedNQEBrowserTest() {
    std::vector<base::Feature> enabled_features = {
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
      // Network Service Sandboxing is unconditionally enabled on these
      // platforms.
      sandbox::policy::features::kNetworkServiceSandbox,
#endif
    };
    scoped_feature_list_.InitWithFeatures(
        enabled_features,
        /*disabled_features=*/{features::kNetworkServiceInProcess});
  }

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    if (!sandbox::policy::features::IsWinNetworkServiceSandboxSupported()) {
      // On *some* Windows, sandboxing cannot be enabled. We skip all the tests
      // on such platforms.
      GTEST_SKIP();
    }
#endif

    // These assertions need to precede ContentBrowserTest::SetUp to prevent the
    // test body from running when one of the assertions fails.
    ASSERT_TRUE(IsOutOfProcessNetworkService());
    ASSERT_TRUE(sandbox::policy::features::IsNetworkSandboxEnabled());

    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SandboxedNQEBrowserTest, GetNetworkService) {
  EXPECT_TRUE(GetNetworkService());
}

// TODO(yoichio): Add more tests.

}  // namespace
}  // namespace content
