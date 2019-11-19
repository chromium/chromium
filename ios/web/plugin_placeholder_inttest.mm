// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPluginNotSupportedText[] =
    "hahaha, your plugin is not supported :D";
}

namespace web {

// Tests that web page shows a placeholder for unsupported plugins.
class PluginPlaceholderTest : public WebTestWithWebState {
 protected:
  PluginPlaceholderTest()
      : WebTestWithWebState(std::make_unique<TestWebClient>()) {
    TestWebClient* web_client = static_cast<TestWebClient*>(GetWebClient());
    web_client->SetPluginNotSupportedText(
        base::UTF8ToUTF16(kPluginNotSupportedText));
  }

  // Sets up |server_| with |html| as response content.
  bool SetUpServer(const std::string& html) WARN_UNUSED_RESULT {
    server_.RegisterDefaultHandler(
        base::BindRepeating(&testing::HandlePageWithHtml, html));
    return server_.Start();
  }

  net::test_server::EmbeddedTestServer server_;
};

// Tests that a large <applet> with text fallback is untouched.
TEST_F(PluginPlaceholderTest, AppletFallback) {
  const char kPageDescription[] = "Applet, text fallback";
  const char kFallbackText[] = "Java? On iOS? C'mon.";
  const std::string page =
      base::StringPrintf("<html><body width='800' height='600'>"
                         "<p>%s</p>"
                         "<applet code='Some.class' width='550' height='550'>"
                         "  <p>%s</p>"
                         "</applet>"
                         "</body></html>",
                         kPageDescription, kFallbackText);
  ASSERT_TRUE(SetUpServer(page));
  test::LoadUrl(web_state(), server_.GetURL("/"));

  // Verify that placeholder image is not displayed.
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPageDescription));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), kFallbackText));
  EXPECT_TRUE(test::WaitForWebViewNotContainingElement(
      web_state(), [ElementSelector selectorWithCSSSelector:"img"]));
  EXPECT_TRUE(test::WaitForWebViewNotContainingText(web_state(),
                                                    kPluginNotSupportedText));
}

// Tests placeholder for a large <applet> with no fallback.
TEST_F(PluginPlaceholderTest, AppletOnly) {
  const char kPageDescription[] = "Applet, no fallback";
  const std::string page =
      base::StringPrintf("<html><body width='800' height='600'>"
                         "<p>%s</p>"
                         "<applet code='Some.class' width='550' height='550'>"
                         "</applet>"
                         "</body></html>",
                         kPageDescription);
  ASSERT_TRUE(SetUpServer(page));
  test::LoadUrl(web_state(), server_.GetURL("/"));

  // Verify that plugin object is replaced with placeholder image.
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPageDescription));
  EXPECT_TRUE(test::WaitForWebViewContainingElement(
      web_state(),
      [ElementSelector selectorWithCSSSelector:"img[src*='data']"]));
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPluginNotSupportedText));
}

// Tests placeholder for a large <object> with a flash embed fallback.
TEST_F(PluginPlaceholderTest, ObjectFlashEmbedFallback) {
  const char kPageDescription[] = "Object, embed fallback";
  const std::string page = base::StringPrintf(
      "<html><body width='800' height='600'>"
      "<p>%s</p>"
      "<object classid='clsid:D27CDB6E-AE6D-11cf-96B8-444553540000'"
      "    codebase='http://download.macromedia.com/pub/shockwave/cabs/'"
      "flash/swflash.cab#version=6,0,0,0' width='550' height='550'>"
      "  <param name='movie' value='some.swf'>"
      "  <embed src='some.swf' type='application/x-shockwave-flash' "
      "width='550' height='550'>"
      "</object>"
      "</body></html>",
      kPageDescription);
  ASSERT_TRUE(SetUpServer(page));
  test::LoadUrl(web_state(), server_.GetURL("/"));

  // Verify that plugin object is replaced with placeholder image.
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPageDescription));
  EXPECT_TRUE(test::WaitForWebViewContainingElement(
      web_state(),
      [ElementSelector selectorWithCSSSelector:"img[src*='data']"]));
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPluginNotSupportedText));
}

// Tests that a large <object> with an embed fallback of unspecified type is
// untouched.
TEST_F(PluginPlaceholderTest, ObjectUndefinedEmbedFallback) {
  const char kPageDescription[] = "Object, embed fallback";
  const std::string page = base::StringPrintf(
      "<html><body width='800' height='600'>"
      "<p>%s</p>"
      "<object classid='clsid:D27CDB6E-AE6D-11cf-96B8-444553540000'"
      "    codebase='http://download.macromedia.com/pub/shockwave/cabs/'"
      "flash/swflash.cab#version=6,0,0,0' width='550' height='550'>"
      "  <param name='movie' value='some.swf'>"
      "  <embed src='some.swf' width='550' height='550'>"
      "</object>"
      "</body></html>",
      kPageDescription);
  ASSERT_TRUE(SetUpServer(page));
  test::LoadUrl(web_state(), server_.GetURL("/"));

  // Verify that placeholder image is not displayed.
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPageDescription));
  EXPECT_TRUE(test::WaitForWebViewNotContainingElement(
      web_state(), [ElementSelector selectorWithCSSSelector:"img"]));
  EXPECT_TRUE(test::WaitForWebViewNotContainingText(web_state(),
                                                    kPluginNotSupportedText));
}

// Tests that a large <object> with text fallback is untouched.
TEST_F(PluginPlaceholderTest, ObjectFallback) {
  const char kPageDescription[] = "Object, text fallback";
  const char kFallbackText[] = "You don't have Flash. Tough luck!";
  const std::string page = base::StringPrintf(
      "<html><body width='800' height='600'>"
      "<p>%s</p>"
      "<object type='application/x-shockwave-flash' data='some.sfw'"
      "    width='550' height='550'>"
      "  <param name='movie' value='some.swf'>"
      "  <p>%s</p>"
      "</object>"
      "</body></html>",
      kPageDescription, kFallbackText);
  ASSERT_TRUE(SetUpServer(page));
  test::LoadUrl(web_state(), server_.GetURL("/"));

  // Verify that placeholder image is not displayed.
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPageDescription));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), kFallbackText));
  EXPECT_TRUE(test::WaitForWebViewNotContainingElement(
      web_state(), [ElementSelector selectorWithCSSSelector:"img"]));
  EXPECT_TRUE(test::WaitForWebViewNotContainingText(web_state(),
                                                    kPluginNotSupportedText));
}

// Tests placeholder for a large <object> with no fallback.
TEST_F(PluginPlaceholderTest, ObjectOnly) {
  const char kPageDescription[] = "Object, no fallback";
  const std::string page = base::StringPrintf(
      "<html><body width='800' height='600'>"
      "<p>%s</p>"
      "<object type='application/x-shockwave-flash' data='some.swf'"
      "    width='550' height='550'>"
      "</object>"
      "</body></html>",
      kPageDescription);
  ASSERT_TRUE(SetUpServer(page));
  test::LoadUrl(web_state(), server_.GetURL("/"));

  // Verify that plugin object is replaced with placeholder image.
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPageDescription));
  EXPECT_TRUE(test::WaitForWebViewContainingElement(
      web_state(),
      [ElementSelector selectorWithCSSSelector:"img[src*='data']"]));
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPluginNotSupportedText));
}

// Tests that a large png <object> is untouched.
TEST_F(PluginPlaceholderTest, PNGObject) {
  const char kPageDescription[] = "PNG object";
  const std::string page = base::StringPrintf(
      "<html><body width='800' height='600'>"
      "<p>%s</p>"
      "<object data='foo.png' type='image/png' width='550' height='550'>"
      "</object>"
      "</body></html>",
      kPageDescription);
  ASSERT_TRUE(SetUpServer(page));
  test::LoadUrl(web_state(), server_.GetURL("/"));

  // Verify that placeholder image is not displayed.
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPageDescription));
  EXPECT_TRUE(test::WaitForWebViewNotContainingElement(
      web_state(), [ElementSelector selectorWithCSSSelector:"img"]));
  EXPECT_TRUE(test::WaitForWebViewNotContainingText(web_state(),
                                                    kPluginNotSupportedText));
}

// Test that non-major plugins (e.g., top/side ads) don't get placeholders.
TEST_F(PluginPlaceholderTest, SmallFlash) {
  const char kPageDescription[] = "Flash ads";
  const std::string page = base::StringPrintf(
      "<html><body width='800' height='600'>"
      "<p>%s</p>"
      // 160x600 "skyscraper"
      "<object classid='clsid:D27CDB6E-AE6D-11cf-96B8-444553540000'"
      "    codebase='http://download.macromedia.com/pub/shockwave/cabs/'"
      "flash/swflash.cab#version=6,0,0,0' width='160' height='600'>"
      "  <param name='movie' value='some.swf'>"
      "  <embed src='some.swf' width='160' height='600'>"
      "</object>"
      // 468x60 "full banner"
      "<object classid='clsid:D27CDB6E-AE6D-11cf-96B8-444553540000'"
      "    codebase='http://download.macromedia.com/pub/shockwave/cabs/'"
      "flash/swflash.cab#version=6,0,0,0' width='468' height='60'>"
      "  <param name='movie' value='some.swf'>"
      "  <embed src='some.swf' width='468' height='60'>"
      "</object>"
      // 728x90 "leaderboard"
      "<object classid='clsid:D27CDB6E-AE6D-11cf-96B8-444553540000'"
      "    codebase='http://download.macromedia.com/pub/shockwave/cabs/'"
      "flash/swflash.cab#version=6,0,0,0' width='728' height='90'>"
      "  <param name='movie' value='some.swf'>"
      "  <embed src='some.swf' width='728' height='90'>"
      "</object>"
      "</body></html>",
      kPageDescription);
  ASSERT_TRUE(SetUpServer(page));
  test::LoadUrl(web_state(), server_.GetURL("/"));

  // Verify that placeholder image is not displayed.
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPageDescription));
  EXPECT_TRUE(test::WaitForWebViewNotContainingElement(
      web_state(), [ElementSelector selectorWithCSSSelector:"img"]));
  EXPECT_TRUE(test::WaitForWebViewNotContainingText(web_state(),
                                                    kPluginNotSupportedText));
}

}  // namespace web
