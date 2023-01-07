// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/js_messaging/java_script_feature_util_impl.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPluginNotSupportedText[] =
    "hahaha, your plugin is not supported :D";
const char16_t kPluginNotSupportedText16[] =
    u"hahaha, your plugin is not supported :D";
}

namespace web {

// Tests that web page shows a placeholder for unsupported plugins.
class PluginPlaceholderTest : public WebTestWithWebState {
 protected:
  PluginPlaceholderTest()
      : WebTestWithWebState(std::make_unique<FakeWebClient>()) {
    FakeWebClient* web_client = static_cast<FakeWebClient*>(GetWebClient());
    web_client->SetPluginNotSupportedText(kPluginNotSupportedText16);
  }

  // Sets up `server_` with `html` as response content.
  [[nodiscard]] bool SetUpServer(const std::string& html) {
    server_.RegisterDefaultHandler(
        base::BindRepeating(&testing::HandlePageWithHtml, html));
    return server_.Start();
  }

  net::test_server::EmbeddedTestServer server_;
};

// Tests that a large <applet> with text fallback is untouched.
TEST_F(PluginPlaceholderTest, AppletFallback) {
  // Plugin Placeholder is no longer used as of iOS 14.5 as <applet> support is
  // completely removed.
  // TODO(crbug.com/1218221): Remove feature once app is iOS 14.5+.
  if (base::ios::IsRunningOnOrLater(14, 5, 0)) {
    return;
  }

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
  ASSERT_TRUE(WaitUntilLoaded());

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
  // Plugin Placeholder is no longer used as of iOS 14.5 as <applet> support is
  // completely removed.
  // TODO(crbug.com/1218221): Remove feature once app is iOS 14.5+.
  if (base::ios::IsRunningOnOrLater(14, 5, 0)) {
    return;
  }

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
  ASSERT_TRUE(WaitUntilLoaded());

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
  // Plugin Placeholder is no longer used as of iOS 14.5 as <applet> support is
  // completely removed.
  // TODO(crbug.com/1218221): Remove feature once app is iOS 14.5+.
  if (base::ios::IsRunningOnOrLater(14, 5, 0)) {
    return;
  }

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
  ASSERT_TRUE(WaitUntilLoaded());

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
  // Plugin Placeholder is no longer used as of iOS 14.5 as <applet> support is
  // completely removed.
  // TODO(crbug.com/1218221): Remove feature once app is iOS 14.5+.
  if (base::ios::IsRunningOnOrLater(14, 5, 0)) {
    return;
  }

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
  ASSERT_TRUE(WaitUntilLoaded());

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
  // Plugin Placeholder is no longer used as of iOS 14.5 as <applet> support is
  // completely removed.
  // TODO(crbug.com/1218221): Remove feature once app is iOS 14.5+.
  if (base::ios::IsRunningOnOrLater(14, 5, 0)) {
    return;
  }

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
  ASSERT_TRUE(WaitUntilLoaded());

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
  // Plugin Placeholder is no longer used as of iOS 14.5 as <applet> support is
  // completely removed.
  // TODO(crbug.com/1218221): Remove feature once app is iOS 14.5+.
  if (base::ios::IsRunningOnOrLater(14, 5, 0)) {
    return;
  }

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
  ASSERT_TRUE(WaitUntilLoaded());

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
  // Plugin Placeholder is no longer used as of iOS 14.5 as <applet> support is
  // completely removed.
  // TODO(crbug.com/1218221): Remove feature once app is iOS 14.5+.
  if (base::ios::IsRunningOnOrLater(14, 5, 0)) {
    return;
  }

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
  ASSERT_TRUE(WaitUntilLoaded());

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
  // Plugin Placeholder is no longer used as of iOS 14.5 as <applet> support is
  // completely removed.
  // TODO(crbug.com/1218221): Remove feature once app is iOS 14.5+.
  if (base::ios::IsRunningOnOrLater(14, 5, 0)) {
    return;
  }

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
  ASSERT_TRUE(WaitUntilLoaded());

  // Verify that placeholder image is not displayed.
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kPageDescription));
  EXPECT_TRUE(test::WaitForWebViewNotContainingElement(
      web_state(), [ElementSelector selectorWithCSSSelector:"img"]));
  EXPECT_TRUE(test::WaitForWebViewNotContainingText(web_state(),
                                                    kPluginNotSupportedText));
}

}  // namespace web
