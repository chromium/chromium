// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_content_rule_list_util.h"

#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/test/test_url_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {
namespace {

using WKContentRuleListUtilTest = PlatformTest;

// Tests that the JSON created for block mode contains the correct keys.
TEST_F(WKContentRuleListUtilTest, LocalResourceJSONBlock) {
  ScopedTestingWebClient web_client(std::make_unique<FakeWebClient>());
  NSString* rules_string = CreateLocalBlockingJsonRuleList();
  NSData* rules_data = [rules_string dataUsingEncoding:NSUTF8StringEncoding];
  id json = [NSJSONSerialization JSONObjectWithData:rules_data
                                            options:0
                                              error:nil];

  // The Apple API says Content Blocker rules must be an array of rules.
  ASSERT_TRUE([json isKindOfClass:[NSArray class]]);

  id block_rule = json[0];
  ASSERT_TRUE([block_rule isKindOfClass:[NSDictionary class]]);
  NSArray* filtered_schemes = @[
    @"file://.*", [@(kTestWebUIScheme) stringByAppendingString:@"://.*"],
    [@(kTestAppSpecificScheme) stringByAppendingString:@"://.*"]
  ];
  ASSERT_NSEQ(filtered_schemes, block_rule[@"trigger"][@"if-top-url"]);
  ASSERT_NSEQ(@"https?://.*", block_rule[@"trigger"][@"url-filter"]);
  NSArray* filtered_types = @[
    @"image", @"style-sheet", @"script", @"font", @"raw", @"svg-document",
    @"media", @"popup", @"ping"
  ];
  ASSERT_NSEQ(filtered_types, block_rule[@"trigger"][@"resource-type"]);
  ASSERT_NSEQ(@"block", block_rule[@"action"][@"type"]);
}

// Tests that the JSON created for mixed content auto-upgrading contains the
// correct keys.
TEST_F(WKContentRuleListUtilTest, AutoUpgradeMixedContent) {
  ScopedTestingWebClient web_client(std::make_unique<FakeWebClient>());
  NSString* rules_string = CreateMixedContentAutoUpgradeJsonRuleList();
  NSData* rules_data = [rules_string dataUsingEncoding:NSUTF8StringEncoding];
  id json = [NSJSONSerialization JSONObjectWithData:rules_data
                                            options:0
                                              error:nil];

  // The Apple API says Content Blocker rules must be an array of rules.
  ASSERT_TRUE([json isKindOfClass:[NSArray class]]);
  id block_rule = json[0];
  ASSERT_TRUE([block_rule isKindOfClass:[NSDictionary class]]);
  NSArray* filtered_schemes = @[ @"https://.*" ];
  ASSERT_NSEQ(filtered_schemes, block_rule[@"trigger"][@"if-top-url"]);

  ASSERT_NSEQ(@"http://.*", block_rule[@"trigger"][@"url-filter"]);
  NSArray* filtered_types = @[ @"image", @"media" ];
  ASSERT_NSEQ(filtered_types, block_rule[@"trigger"][@"resource-type"]);
  ASSERT_NSEQ(@"make-https", block_rule[@"action"][@"type"]);
}

}  // namespace
}  // namespace web
