// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/extension/gpc_web_extension/gpc_web_extension.h"

#import <WebKit/WebKit.h>

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {

class GPCWebExtensionTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;
};

// Tests loading the bundled GPC WebExtension singleton.
TEST_F(GPCWebExtensionTest, TestLoadGPCWebExtension) API_AVAILABLE(ios(18.4)) {
  GPCWebExtension* extension = GPCWebExtension::GetInstance();
  ASSERT_NE(nullptr, extension);
  EXPECT_NE(nil, extension->GetExtensionURL());

  base::test::TestFuture<WKWebExtension*> future;
  extension->CreateWKWebExtension(future.GetCallback());
  WKWebExtension* result = future.Get();
  ASSERT_NE(nil, result);
  EXPECT_EQ(result, extension->GetWKWebExtension());
  EXPECT_TRUE(result.hasInjectedContent);
  EXPECT_TRUE(result.hasContentModificationRules);
  EXPECT_NSEQ(@"Sec-GPC", result.displayName);
}

}  // namespace web
