// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"

#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// Test fixture for page_context_utils.
class PageContextUtilsTest : public PlatformTest {
 protected:
  web::FakeWebState* web_state() { return &web_state_; }

 private:
  web::FakeWebState web_state_;
};

// Tests that CanExtractPageContextForWebState returns true for HTTP URLs with
// HTML content type.
TEST_F(PageContextUtilsTest, TestHttpUrlAndHtmlContentType) {
  web_state()->SetCurrentURL(GURL("http://www.example.com"));
  web_state()->SetContentsMimeType("text/html");
  EXPECT_TRUE(CanExtractPageContextForWebState(web_state()));
}

// Tests that CanExtractPageContextForWebState returns true for HTTPS URLs with
// HTML content type.
TEST_F(PageContextUtilsTest, TestHttpsUrlAndHtmlContentType) {
  web_state()->SetCurrentURL(GURL("https://www.example.com"));
  web_state()->SetContentsMimeType("text/html");
  EXPECT_TRUE(CanExtractPageContextForWebState(web_state()));
}

// Tests that CanExtractPageContextForWebState returns true for HTTP URLs with
// image content type.
TEST_F(PageContextUtilsTest, TestHttpUrlAndImageContentType) {
  web_state()->SetCurrentURL(GURL("http://www.example.com"));
  web_state()->SetContentsMimeType("image/png");
  EXPECT_TRUE(CanExtractPageContextForWebState(web_state()));
}

// Tests that CanExtractPageContextForWebState returns false for non-HTTP/HTTPS
// URLs.
TEST_F(PageContextUtilsTest, TestNonHttpUrl) {
  web_state()->SetCurrentURL(GURL("chrome://newtab"));
  web_state()->SetContentsMimeType("text/html");
  EXPECT_FALSE(CanExtractPageContextForWebState(web_state()));
}

// Tests that CanExtractPageContextForWebState returns false for non-HTML/image
// content types.
TEST_F(PageContextUtilsTest, TestNonHtmlOrImageContentType) {
  web_state()->SetCurrentURL(GURL("https://www.example.com"));
  web_state()->SetContentsMimeType("application/pdf");
  EXPECT_FALSE(CanExtractPageContextForWebState(web_state()));
}

// Tests that CanExtractPageContextForWebState returns false for null
// web_state.
TEST_F(PageContextUtilsTest, TestNullWebState) {
  EXPECT_FALSE(CanExtractPageContextForWebState(nullptr));
}
