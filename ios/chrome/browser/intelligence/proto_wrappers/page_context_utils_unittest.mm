// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"

#import "base/strings/string_number_conversions.h"
#import "base/unguessable_token.h"
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

// Tests DeserializeFrameIdAsLocalFrameToken with a valid ID.
TEST_F(PageContextUtilsTest, TestDeserializeLocalFrameTokenValid) {
  // 32-character hex string representing a 128-bit token.
  std::string valid_id = "0123456789ABCDEF0123456789ABCDEF";
  std::optional<autofill::LocalFrameToken> token =
      DeserializeFrameIdAsLocalFrameToken(valid_id);
  EXPECT_TRUE(token.has_value());
  EXPECT_FALSE(token->value().is_empty());
}

// Tests DeserializeFrameIdAsLocalFrameToken with an invalid ID.
TEST_F(PageContextUtilsTest, TestDeserializeLocalFrameTokenInvalid) {
  std::string invalid_id = "invalid-token";
  std::optional<autofill::LocalFrameToken> token =
      DeserializeFrameIdAsLocalFrameToken(invalid_id);
  EXPECT_FALSE(token.has_value());
}

// Tests DeserializeFrameIdAsRemoteFrameToken with a valid ID.
TEST_F(PageContextUtilsTest, TestDeserializeRemoteFrameTokenValid) {
  // 32-character hex string representing a 128-bit token.
  std::string valid_id = "FEDCBA9876543210FEDCBA9876543210";
  std::optional<autofill::RemoteFrameToken> token =
      DeserializeFrameIdAsRemoteFrameToken(valid_id);
  EXPECT_TRUE(token.has_value());
  EXPECT_FALSE(token->value().is_empty());
}

// Tests DeserializeFrameIdAsRemoteFrameToken with an invalid ID.
TEST_F(PageContextUtilsTest, TestDeserializeRemoteFrameTokenInvalid) {
  std::string invalid_id = "";
  std::optional<autofill::RemoteFrameToken> token =
      DeserializeFrameIdAsRemoteFrameToken(invalid_id);
  EXPECT_FALSE(token.has_value());
}
