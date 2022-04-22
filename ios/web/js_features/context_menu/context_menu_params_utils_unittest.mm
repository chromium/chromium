// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_params_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/web/common/referrer_util.h"
#include "ios/web/js_features/context_menu/context_menu_constants.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Text values for the tapped element triggering the context menu.
const char kLinkUrl[] = "http://link.url/";
const char kSrcUrl[] = "http://src.url/";
const char kTitle[] = "title";
const char kReferrerPolicy[] = "always";
const char kLinkText[] = "link text";
const char kAlt[] = "alt text";
const double kNaturalWidth = 200.0;
const double kNaturalHeight = 300.0;
const double kBoundingBoxX = 10.0;
const double kBoundingBoxY = 20.0;
const double kBoundingBoxWidth = 50.0;
const double kBoundingBoxHeight = 200.0;

// Returns true if the |params| contain enough information to present a context
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
  EXPECT_NEAR(params.natural_width, 0.0, DBL_EPSILON);
  EXPECT_NEAR(params.natural_height, 0.0, DBL_EPSILON);
  EXPECT_TRUE(CGRectIsEmpty(params.bounding_box));
  EXPECT_EQ(params.screenshot, nil);
}

// Tests the parsing of the element NSDictionary.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTest) {
  base::Value element_dict(base::Value::Type::DICTIONARY);
  element_dict.SetStringKey(kContextMenuElementHyperlink, kLinkUrl);
  element_dict.SetStringKey(kContextMenuElementSource, kSrcUrl);
  element_dict.SetStringKey(kContextMenuElementTitle, kTitle);
  element_dict.SetStringKey(kContextMenuElementReferrerPolicy, kReferrerPolicy);
  element_dict.SetStringKey(kContextMenuElementInnerText, kLinkText);
  element_dict.SetStringKey(kContextMenuElementAlt, kAlt);
  element_dict.SetDoubleKey(kContextMenuElementNaturalWidth, kNaturalWidth);
  element_dict.SetDoubleKey(kContextMenuElementNaturalHeight, kNaturalHeight);
  base::Value bounding_box_element_dict(base::Value::Type::DICTIONARY);
  bounding_box_element_dict.SetDoubleKey(kContextMenuElementBoundingBoxX,
                                         kBoundingBoxX);
  bounding_box_element_dict.SetDoubleKey(kContextMenuElementBoundingBoxY,
                                         kBoundingBoxY);
  bounding_box_element_dict.SetDoubleKey(kContextMenuElementBoundingBoxWidth,
                                         kBoundingBoxWidth);
  bounding_box_element_dict.SetDoubleKey(kContextMenuElementBoundingBoxHeight,
                                         kBoundingBoxHeight);
  element_dict.SetKey(kContextMenuElementBoundingBox,
                      std::move(bounding_box_element_dict));
  ContextMenuParams params =
      ContextMenuParamsFromElementDictionary(&element_dict);

  EXPECT_TRUE(params.is_main_frame);
  EXPECT_EQ(params.link_url, GURL(kLinkUrl));
  EXPECT_EQ(params.src_url, GURL(kSrcUrl));
  EXPECT_NSEQ(params.text, @(kLinkText));
  EXPECT_EQ(params.referrer_policy, ReferrerPolicyFromString(kReferrerPolicy));

  EXPECT_EQ(params.view, nil);
  EXPECT_TRUE(CGPointEqualToPoint(params.location, CGPointZero));

  EXPECT_NSEQ(params.title_attribute, @(kTitle));
  EXPECT_NSEQ(params.alt_text, @(kAlt));

  EXPECT_NEAR(params.natural_width, kNaturalWidth, DBL_EPSILON);
  EXPECT_NEAR(params.natural_height, kNaturalHeight, DBL_EPSILON);

  EXPECT_NEAR(params.bounding_box.origin.x, kBoundingBoxX, DBL_EPSILON);
  EXPECT_NEAR(params.bounding_box.origin.y, kBoundingBoxY, DBL_EPSILON);
  EXPECT_NEAR(params.bounding_box.size.width, kBoundingBoxWidth, DBL_EPSILON);
  EXPECT_NEAR(params.bounding_box.size.height, kBoundingBoxHeight, DBL_EPSILON);
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
