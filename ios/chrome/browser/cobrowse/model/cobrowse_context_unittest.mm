// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "net/base/url_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

const char kDefaultURL[] = "https://www.google.com/search?q=test";
const char kOverrideURL[] = "https://www.overridden.com/search?q=test";
NSString* const kCobrowseGwsURLKey = @"CobrowseGwsURL";

class CobrowseContextTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kCobrowseGwsURLKey];
    [[NSUserDefaults standardUserDefaults] synchronize];
  }

  void TearDown() override {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kCobrowseGwsURLKey];
    [[NSUserDefaults standardUserDefaults] synchronize];
    PlatformTest::TearDown();
  }
};

// Tests that the context is initialized with the provided URL when no override
// is present.
TEST_F(CobrowseContextTest, InitWithDefaultURL) {
  GURL url(kDefaultURL);
  CobrowseContext* context = [[CobrowseContext alloc] initWithURL:url];

  EXPECT_EQ(context.url.host(), "www.google.com");
  EXPECT_EQ(context.url.path(), "/search");

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "gsc", &value));
  EXPECT_EQ(value, "2");
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "sourceid", &value));
  EXPECT_EQ(value, "chrome-mobile");
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "gsas", &value));
  EXPECT_EQ(value, "4");
}

// Tests that the context is initialized with the override URL when it is
// present in NSUserDefaults.
TEST_F(CobrowseContextTest, InitWithOverrideURL) {
  [[NSUserDefaults standardUserDefaults]
      setObject:base::SysUTF8ToNSString(kOverrideURL)
         forKey:kCobrowseGwsURLKey];
  [[NSUserDefaults standardUserDefaults] synchronize];

  GURL url(kDefaultURL);
  CobrowseContext* context = [[CobrowseContext alloc] initWithURL:url];

  EXPECT_EQ(context.url.host(), "www.overridden.com");
  EXPECT_EQ(context.url.path(), "/search");

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "gsc", &value));
  EXPECT_EQ(value, "2");
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "sourceid", &value));
  EXPECT_EQ(value, "chrome-mobile");
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "gsas", &value));
  EXPECT_EQ(value, "4");
}

}  // namespace
