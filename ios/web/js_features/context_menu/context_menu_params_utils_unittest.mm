// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_params_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/web/common/referrer_util.h"
#import "ios/web/js_features/context_menu/context_menu_constants.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
// Text values for the tapped element triggering the context menu.
const char kLinkUrl[] = "http://link.url/";
const char kSrcUrl[] = "http://src.url/";
const char kTitle[] = "title";
const char kReferrerPolicy[] = "always";
const char kLinkText[] = "link text";
const char kAlt[] = "alt text";

// Returns true if the `params` contain enough information to present a context
// menu. (A valid url for either link_url or src_url must exist in the params.)
bool CanShowContextMenuForParams(const web::ContextMenuParams& params) {
  if (params.link_url.is_valid()) {
    return true;
  }
  if (params.src_url.is_valid()) {
    return true;
  }
  return false;
}
}

namespace web {

// Test fixture for ContextMenuParams utilities.
typedef PlatformTest ContextMenuParamsUtilsTest;

// Tests the empty contructor.
TEST_F(ContextMenuParamsUtilsTest, EmptyParams) {
  ContextMenuParams params;
  EXPECT_TRUE(params.is_main_frame);
  EXPECT_FALSE(params.link_url.is_valid());
  EXPECT_FALSE(params.src_url.is_valid());
  EXPECT_EQ(params.referrer_policy, ReferrerPolicyDefault);
  EXPECT_EQ(params.view, nil);
  EXPECT_TRUE(CGPointEqualToPoint(params.location, CGPointZero));
  EXPECT_NSEQ(params.text, nil);
  EXPECT_NSEQ(params.title_attribute, nil);
  EXPECT_NSEQ(params.alt_text, nil);
}

// Tests the parsing of the element NSDictionary.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTest) {
  auto element_dict =
      base::Value::Dict()
          .Set(kContextMenuElementHyperlink, kLinkUrl)
          .Set(kContextMenuElementSource, kSrcUrl)
          .Set(kContextMenuElementTitle, kTitle)
          .Set(kContextMenuElementReferrerPolicy, kReferrerPolicy)
          .Set(kContextMenuElementInnerText, kLinkText)
          .Set(kContextMenuElementAlt, kAlt);
  ContextMenuParams params =
      ContextMenuParamsFromElementDictionary(element_dict);

  EXPECT_TRUE(params.is_main_frame);
  EXPECT_EQ(params.link_url, GURL(kLinkUrl));
  EXPECT_EQ(params.src_url, GURL(kSrcUrl));
  EXPECT_NSEQ(params.text, @(kLinkText));
  EXPECT_EQ(params.referrer_policy, ReferrerPolicyFromString(kReferrerPolicy));

  EXPECT_EQ(params.view, nil);
  EXPECT_TRUE(CGPointEqualToPoint(params.location, CGPointZero));

  EXPECT_NSEQ(params.title_attribute, @(kTitle));
  EXPECT_NSEQ(params.alt_text, @(kAlt));
}


// Tests that a context menu will not be shown for empty params.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestEmptyDictionary) {
  EXPECT_FALSE(CanShowContextMenuForParams(ContextMenuParams()));
}

// Tests that a context menu will be shown for a link.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestHyperlink) {
  ContextMenuParams params;
  params.link_url = GURL("http://example.com");
  params.text = @"Click me.";
  EXPECT_TRUE(CanShowContextMenuForParams(params));
}

// Tests that a context menu will not be shown for an invalid link.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestInvalidHyperlink) {
  ContextMenuParams params;
  params.link_url = GURL("invalid_url");
  EXPECT_FALSE(CanShowContextMenuForParams(params));
}

// Tests that a context menu will be shown for an image.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestImageWithTitle) {
  ContextMenuParams params;
  params.src_url = GURL("http://example.com/image.jpeg");
  EXPECT_TRUE(CanShowContextMenuForParams(params));
}

// Tests that a context menu will not be shown for an image with an invalid
// source url.
TEST_F(ContextMenuParamsUtilsTest,
       CanShowContextMenuTestImageWithInvalidSource) {
  ContextMenuParams params;
  params.src_url = GURL("invalid_url");
  EXPECT_FALSE(CanShowContextMenuForParams(params));
}

// Tests that a context menu will be shown for a linked image.
TEST_F(ContextMenuParamsUtilsTest, CanShowContextMenuTestLinkedImage) {
  ContextMenuParams params;
  params.link_url = GURL("http://example.com");
  params.src_url = GURL("http://example.com/image.jpeg");
  EXPECT_TRUE(CanShowContextMenuForParams(params));
}

}  // namespace web
