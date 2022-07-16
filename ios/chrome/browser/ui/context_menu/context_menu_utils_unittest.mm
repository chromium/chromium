// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/web/common/features.h"
#include "ios/web/common/referrer_util.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kJavaScriptLinkUrl[] = "javascript://src.url/";
const char kDataUrl[] = "data://foo.bar/";
const char kLinkUrl[] = "http://www.link.url/test";
const char kSrcUrl[] = "http://src.url/";
const char kTitle[] = "title";
const char kAltText[] = "alt text";
}  // namespace

typedef PlatformTest ContextMenuUtilsTest;

// Tests title is set as the formatted URL when there is no title.
TEST_F(ContextMenuUtilsTest, TitleNoTitle) {
  web::ContextMenuParams params;
  params.link_url = GURL(kLinkUrl);

  std::u16string urlText = url_formatter::FormatUrl(GURL(kLinkUrl));
  NSString* title = base::SysUTF16ToNSString(urlText);

  EXPECT_NSEQ(GetContextMenuTitle(params), title);
  EXPECT_FALSE(IsImageTitle(params));
}

// Tests title is set to "JavaScript" if there is no title and "href" links to
// JavaScript URL.
TEST_F(ContextMenuUtilsTest, TitleJavascriptTitle) {
  web::ContextMenuParams params;
  params.link_url = GURL(kJavaScriptLinkUrl);

  EXPECT_NSEQ(GetContextMenuTitle(params), @"JavaScript");
  EXPECT_FALSE(IsImageTitle(params));
}

// Tests title is set to |src_url| if there is no title.
TEST_F(ContextMenuUtilsTest, TitleSrcTitle) {
  web::ContextMenuParams params;
  params.src_url = GURL(kSrcUrl);

  EXPECT_NSEQ(GetContextMenuTitle(params), @(kSrcUrl));
  EXPECT_FALSE(IsImageTitle(params));
}

// Tests title is set to nil if there is no title and src is a data URL.
TEST_F(ContextMenuUtilsTest, TitleDataTitle) {
  web::ContextMenuParams params;
  params.src_url = GURL(kDataUrl);

  EXPECT_NSEQ(GetContextMenuTitle(params), nil);
  EXPECT_FALSE(IsImageTitle(params));
}

// Tests that the menu title prepends the element's alt text if it is an image
// without a link.
TEST_F(ContextMenuUtilsTest, TitlePrependAltForImage) {
  web::ContextMenuParams params;
  params.alt_text = @(kAltText);
  params.src_url = GURL(kSrcUrl);

  EXPECT_TRUE([GetContextMenuTitle(params) hasPrefix:@(kAltText)]);
  EXPECT_TRUE([GetContextMenuTitle(params) hasSuffix:@(kSrcUrl)]);
  EXPECT_FALSE(IsImageTitle(params));
}

// Tests that the menu title prepends the element's alt text if it is an image
// without a link.
TEST_F(ContextMenuUtilsTest, TitlePrependAltForImageWithTitle) {
  web::ContextMenuParams params;
  params.src_url = GURL(kSrcUrl);
  params.title_attribute = @(kTitle);
  params.alt_text = @(kAltText);

  EXPECT_TRUE([GetContextMenuTitle(params) hasPrefix:@(kAltText)]);
  EXPECT_TRUE([GetContextMenuTitle(params) hasSuffix:@(kTitle)]);
  EXPECT_TRUE(IsImageTitle(params));
}

// Tests that a link with HTTP scheme returns the simplified domain as title.
TEST_F(ContextMenuUtilsTest, TitleForHTTPLink) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kWebViewNativeContextMenuPhase2);

  web::ContextMenuParams params;
  params.link_url = GURL(kLinkUrl);
  params.title_attribute = @(kTitle);

  EXPECT_NSEQ(@"link.url", GetContextMenuTitle(params));
}

// Tests that a link with a JavaScript scheme returns the scheme.
TEST_F(ContextMenuUtilsTest, TitleForJavaScript) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kWebViewNativeContextMenuPhase2);

  web::ContextMenuParams params;
  params.link_url = GURL(kJavaScriptLinkUrl);
  params.title_attribute = @(kTitle);

  EXPECT_NSEQ(@"javascript", GetContextMenuTitle(params));
}

// Tests that a link with a data scheme returns the scheme.
TEST_F(ContextMenuUtilsTest, TitleForData) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kWebViewNativeContextMenuPhase2);

  web::ContextMenuParams params;
  params.link_url = GURL(kDataUrl);
  params.title_attribute = @(kTitle);

  EXPECT_NSEQ(@"data", GetContextMenuTitle(params));
}

// Tests that the title is returned when there is no alt text.
TEST_F(ContextMenuUtilsTest, TitleForTitle) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kWebViewNativeContextMenuPhase2);

  web::ContextMenuParams params;
  params.src_url = GURL(kSrcUrl);
  params.title_attribute = @(kTitle);

  EXPECT_NSEQ(@(kTitle), GetContextMenuTitle(params));
}

// Tests that the alt text is returned when there is no title.
TEST_F(ContextMenuUtilsTest, TitleForAlt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kWebViewNativeContextMenuPhase2);

  web::ContextMenuParams params;
  params.src_url = GURL(kSrcUrl);
  params.alt_text = @(kAltText);

  EXPECT_NSEQ(@(kAltText), GetContextMenuTitle(params));
}

// Tests that the title and the alt text are returned.
TEST_F(ContextMenuUtilsTest, TitleForTitleAndAlt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kWebViewNativeContextMenuPhase2);

  web::ContextMenuParams params;
  params.src_url = GURL(kSrcUrl);
  params.title_attribute = @(kTitle);
  params.alt_text = @(kAltText);

  NSString* expected = [NSString stringWithFormat:@"%s – %s", kAltText, kTitle];
  EXPECT_NSEQ(expected, GetContextMenuTitle(params));
}
