// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <vector>

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/js_features/context_menu/context_menu_constants.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/test/fakes/crw_fake_script_message_handler.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Unit tests for ios/web/web_state/js/resources/context_menu.js.

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Request id used for __gCrWeb.findElementAtPoint call.
const char kRequestId[] = "UNIQUE_IDENTIFIER";

// The base url for loaded web pages.
const char kTestUrl[] = "https://chromium.test/";

// A point in the web view's coordinate space on the link returned by
// `GetHtmlForLink()`.
const CGPoint kPointOnLink = {5.0, 2.0};

// A point in the web view's coordinate space on the image returned by
// `GetHtmlForImage()`.
const CGPoint kPointOnImage = {50.0, 10.0};
// A point in the web view's coordinate space within the document bounds but not
// on the image returned by `GetHtmlForImage()`.
const CGPoint kPointOutsideImage = {50.0, 100.0};

// A point in the web view's coordinate space on the svg link returned by
// `GetHtmlForSvgLink()` and `GetHtmlForSvgXlink()`.
const CGPoint kPointOnSvgLink = {50.0, 75.0};
// A point in the web view's coordinate space within the svg element but not
// on the svg link returned by `GetHtmlForSvgLink()` and `GetHtmlForSvgXlink()`.
const CGPoint kPointOutsideSvgLink = {50.0, 10.0};

// A point in the web view's coordinate space on the shadow DOM link returned by
// `GetHtmlForShadowDomLink()`.
const CGPoint kPointOnShadowDomLink = {5.0, 2.0};
// A point in the web view's coordinate space within the shadow DOM returned by
// `GetHtmlForShadowDomLink()` but not on the link.
const CGPoint kPointOutsideShadowDomLink = {50.0, 75.0};

// A point in the web view's coordinate space outside of the document bounds.
const CGPoint kPointOutsideDocument = {150.0, 150.0};

// A point in the web view's coordinate space inside the surrounding text.
const CGPoint kPointInsideSurroundingText = {90.0, 90.0};

// A base64 encoded gif image of a single white pixel.
const char kFallbackImageSource[] = "data:image/gif;base64,R0lGODlhAQABAIAAAP7/"
                                    "/wAAACH5BAAAAAAALAAAAAABAAEAAAICRAEAOw==";

// A base64 encoded svg image of a 600x600 blue square.
const char kImageSource[] =
    "data:image/"
    "svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiI"
    "HN0YW5kYWxvbmU9InllcyI/"
    "Pgo8c3ZnIHdpZHRoPSI2MDAiIGhlaWdodD0iNjAwIiB4bWxucz0i"
    "aHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZlcnNpb249IjEuMSI+"
    "CjxyZWN0IHdpZHRoPSI"
    "2MDAiIGhlaWdodD0iNjAwIiBmaWxsPSIjMDA2NmZmIi8+Cjwvc3ZnPg==";

// Alt text on image element for accessibility.
const char kImageAlt[] = "Some alt text for an image";

// Style used to size the image returned by `GetHtmlForImage()`.
const char kImageSizeStyle[] = "width:100%;height:25%;";

// Style used to size a div with the background image set.
const char kBackgroundDivStyle[] = "width:100%;height:25px;";

// Style used to create an overlay div.
const char kOverlayDivStyle[] = "position:fixed;left:0;top:0;width:100%;height:"
                                "100%;background-color:black;";

// Returns HTML for a test webpage with the given `head` and `body`.
NSString* GetHtmlForPage(NSString* head, NSString* body) {
  return [NSString
      stringWithFormat:
          @"<html><head>"
           "<style>body { font-size:14em; }</style>"
           "<meta name=\"viewport\" content=\"user-scalable=no, width=100\">"
           "%@"
           "</head><body><p>%@</p></body></html>",
          head ? head : @"", body];
}

// Returns HTML for a link to `href`, display `text`, and inline `style`.
NSString* GetHtmlForLink(const char* href,
                         const char* text,
                         const char* style) {
  std::string style_attribute =
      style ? base::StringPrintf("style=\"%s\" ", style) : "";
  return [NSString stringWithFormat:@"<a %shref=\"%s\">%s</a>",
                                    style_attribute.c_str(), href, text];
}

// Returns HTML for an SVG shape which links to `href`.
NSString* GetHtmlForSvgLink(const char* href) {
  NSString* svg_shape = @"<rect y=\"50\" width=\"100\" height=\"50\"/>";
  return [NSString
      stringWithFormat:
          @"<svg width=\"100\" height=\"100\"><a href=\"%s\">%@</a></svg>",
          href, svg_shape];
}

// Returns HTML for an SVG shape which links to `href` with an xlink:href.
NSString* GetHtmlForSvgXlink(const char* href) {
  NSString* svg_shape = @"<rect y=\"50\" width=\"100\" height=\"50\"/>";
  return [NSString stringWithFormat:@"<svg width=\"100\" height=\"100\"><a "
                                    @"xlink:href=\"%s\">%@</a></svg>",
                                    href, svg_shape];
}

// Returns HTML for a link to `href` and display text `text`.
NSString* GetHtmlForLink(const char* href, NSString* text) {
  return GetHtmlForLink(href, base::SysNSStringToUTF8(text).c_str(),
                        /*style=*/nullptr);
}

// Returns HTML for a shadow DOM link to `href` and display text `text`.
NSString* GetHtmlForShadowDomLink(const char* href, NSString* text) {
  NSString* shadow_html = [NSString
      stringWithFormat:@"<div style=\"height:100px;font-size:20px\">%@</div>",
                       GetHtmlForLink(href, text)];
  return [NSString
      stringWithFormat:
          @"<div id='largeDiv' style='height:100px'></div>"
          @"<script>var shadow = "
          @"document.getElementById('largeDiv').attachShadow({mode: 'open'});"
          @"shadow.innerHTML = '%@';"
          @"</script>",
          shadow_html];
}

// Returns html for an image styled to fill the width and top 25% of its
// container. `source` must be provided, but specifying an image `title` and
// inline `style` are optional.
NSString* GetHtmlForImage(const char* source,
                          const char* alt_text,
                          const char* title,
                          const char* style) {
  const char* additional_css = style ? style : "";
  NSString* image_title =
      title ? [NSString stringWithFormat:@"title='%s' ", title] : @"";
  return [NSString
      stringWithFormat:@"<img id='image' %@style='%s%s' src='%s' alt='%s'/>",
                       image_title, kImageSizeStyle, additional_css, source,
                       alt_text];
}

// Returns html for an image styled to fill the width and top 25% of its
// container.
NSString* GetHtmlForImage() {
  return GetHtmlForImage(kImageSource, kImageAlt, /*title=*/nullptr,
                         /*style=*/nullptr);
}

// Returns html for an image styled to fill the width and top 25% of its
// container.
NSString* ImageHtmlWithSource(const char* source) {
  return GetHtmlForImage(source, kImageAlt, /*title=*/nullptr,
                         /*style=*/nullptr);
}

}  // namespace

namespace web {

// Test fixture to test __gCrWeb.findElementAtPoint function defined in
// context_menu.js.
class ContextMenuJsFindElementAtPointTest : public PlatformTest {
 public:
  ContextMenuJsFindElementAtPointTest()
      : script_message_handler_([[CRWFakeScriptMessageHandler alloc] init]),
        web_view_([[WKWebView alloc]
            initWithFrame:CGRectMake(0.0, 0.0, 100.0, 100.0)]) {
    [web_view_.configuration.userContentController
        addScriptMessageHandler:script_message_handler_
                           name:@"FindElementResultHandler"];

    WKUserScript* shared_scripts = [[WKUserScript alloc]
          initWithSource:web::test::GetSharedScripts()
           injectionTime:WKUserScriptInjectionTimeAtDocumentStart
        forMainFrameOnly:NO];
    [web_view_.configuration.userContentController
        addUserScript:shared_scripts];

    WKUserScript* all_frames_script = [[WKUserScript alloc]
          initWithSource:test::GetPageScript(@"all_frames_context_menu")
           injectionTime:WKUserScriptInjectionTimeAtDocumentStart
        forMainFrameOnly:NO];
    [web_view_.configuration.userContentController
        addUserScript:all_frames_script];

    WKUserScript* main_frame_script = [[WKUserScript alloc]
          initWithSource:test::GetPageScript(@"main_frame_context_menu")
           injectionTime:WKUserScriptInjectionTimeAtDocumentStart
        forMainFrameOnly:YES];
    [web_view_.configuration.userContentController
        addUserScript:main_frame_script];
  }

 protected:
  // Returns details of the DOM element at the given `point` in the web view
  // viewport's coordinate space.
  base::Value FindElementAtPoint(CGPoint point, BOOL surroundingTextEnabled) {
    bool gCrWeb_injected = web::test::WaitForInjectedScripts(web_view_);
    if (!gCrWeb_injected) {
      // This EXPECT_TRUE call will always fail. However, add the conditional to
      // also return null and prevent further execution of this method.
      EXPECT_TRUE(gCrWeb_injected);
      return base::Value();
    }

    // Force layout
    web::test::ExecuteJavaScript(web_view_,
                                 @"document.getElementsByTagName('p')");

    // Clear previous script message response.
    script_message_handler_.lastReceivedScriptMessage = nil;

    ExecuteFindElementFromPointJavaScript(point, surroundingTextEnabled);

    // Wait for response.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return !!script_message_handler_.lastReceivedScriptMessage;
    }));

    if (!script_message_handler_.lastReceivedScriptMessage) {
      return base::Value();
    }
    return web::ValueResultFromWKResult(
               script_message_handler_.lastReceivedScriptMessage.body)
        ->Clone();
  }

  // Finds the element at the given `point` and compares it against
  // `expected_result`. Retries if there is a mismatch.  Without
  // the retry logic, these tests fail flakily, possibly because they attempt to
  // find the element before the webview has completed layout and/or
  // rendering. This occurs on all iOS versions, but seems to be worse on iOS
  // 15. Adding a fixed delay seems to give the webview enough time to make
  // itself ready for the test, but retrying allows for the delay to be as short
  // as possible.
  // TODO(crbug.com/1219869): Find a better "ready" signal for the webview and
  // remove this retry logic.
  void CheckElementResult(CGPoint point,
                          const base::Value& expected_result,
                          const std::vector<const char*>& ignored_keys,
                          BOOL surroundingTextEnabled) {
    constexpr int kNumTries = 13;
    for (int i = 0; i < kNumTries; ++i) {
      base::Value result = FindElementAtPoint(point, surroundingTextEnabled);
      for (const char* key : ignored_keys) {
        result.RemoveKey(key);
      }
      if (result == expected_result) {
        return;
      } else if (i == kNumTries - 1) {
        ASSERT_EQ(result, expected_result);
      }
    }
  }

  void CheckElementResult(CGPoint point,
                          const base::Value& expected_result,
                          BOOL surroundingTextEnabled) {
    return CheckElementResult(point, expected_result,
                              std::vector<const char*>(),
                              surroundingTextEnabled);
  }

  // Returns web view's content size from the current web state.
  CGSize GetWebViewContentSize() { return web_view_.scrollView.contentSize; }

  // Returns the test page URL.
  NSURL* GetTestURL() { return net::NSURLWithGURL(GURL(kTestUrl)); }

  // Executes __gCrWeb.findElementAtPoint script with the given `point` in the
  // web view viewport's coordinate space.
  id ExecuteFindElementFromPointJavaScript(CGPoint point,
                                           BOOL surroundingTextEnabled) {
    CGSize size = GetWebViewContentSize();
    const char* enableSurroundingText =
        surroundingTextEnabled ? "true" : "false";
    NSString* script =
        [NSString stringWithFormat:
                      @"__gCrWeb.findElementAtPoint('%s', %g, %g, %g, %g, %s)",
                      kRequestId, point.x, point.y, size.width, size.height,
                      enableSurroundingText];

    return web::test::ExecuteJavaScript(web_view_, script);
  }

  // Handles script message responses sent from `web_view_`.
  CRWFakeScriptMessageHandler* script_message_handler_;

  // The web view used for testing.
  WKWebView* web_view_;
};

#pragma mark - Long press with Surrounding text enabled

TEST_F(ContextMenuJsFindElementAtPointTest, FetchSurroundingTextEnabled) {
  NSString* html =
      @"<html><head>"
       "<meta name=\"viewport\" content=\"user-scalable=no, width=100\">"
       "</head><body><div>This is the address's first line</div>"
       "<p>Lorem ipsum<span>dolor sit amet. 49 WEST</span>27TH STREET "
       "reprehenderit sed cumque magni ut omnis sint est deserunt eveniet non "
       "omnis esse et debitis labore et Quis consequatur.</p>"
       "</body></html>";

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementTagName, "P");
  expected_value.SetStringKey(
      kContextMenuElementSurroundingText,
      "This is the address's first line Lorem ipsum dolor sit amet. 49 WEST "
      "27TH STREET reprehenderit sed cumque magni ut omnis sint est deserunt "
      "e");

  std::vector<const char*> ignored_keys;
  ignored_keys.push_back(kContextMenuElementInnerText);
  ignored_keys.push_back(kContextMenuElementTextOffset);
  ignored_keys.push_back(kContextMenuElementSurroundingTextOffset);
  CheckElementResult(kPointInsideSurroundingText, expected_value, ignored_keys,
                     true);
}

#pragma mark - Long press with Surrounding text disabled

TEST_F(ContextMenuJsFindElementAtPointTest, FetchSurroundingTextDisabled) {
  NSString* html =
      @"<html><head>"
       "<meta name=\"viewport\" content=\"user-scalable=no, width=100\">"
       "</head><body><div>This is the address's first line</div>"
       "<p>Lorem ipsum<span>dolor sit amet. 49 WEST</span>27TH STREET "
       "reprehenderit sed cumque magni ut omnis sint est deserunt eveniet non "
       "omnis esse et debitis labore et Quis consequatur.</p>"
       "</body></html>";

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementTagName, "P");

  std::vector<const char*> ignored_keys;
  ignored_keys.push_back(kContextMenuElementInnerText);
  ignored_keys.push_back(kContextMenuElementTextOffset);
  ignored_keys.push_back(kContextMenuElementSurroundingTextOffset);
  CheckElementResult(kPointInsideSurroundingText, expected_value, ignored_keys,
                     false);
}

#pragma mark - Image without link

// Tests that the correct src and referrer are found for an image.
TEST_F(ContextMenuJsFindElementAtPointTest, FindImageElementAtPoint) {
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForImage());
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, kImageSource);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that the correct src is found for picture elements.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageElementAtPointInPictureElement) {
  NSString* backing_image_html = GetHtmlForImage(
      kFallbackImageSource, kImageAlt, /*title=*/nullptr, /*style=*/nullptr);
  NSString* html_for_picture = [NSString
      stringWithFormat:@"<picture style='%s'><source media='(min-width:0px)' "
                       @"srcset='%s'>%@</picture>",
                       kImageSizeStyle, kImageSource, backing_image_html];
  NSString* html = GetHtmlForPage(/*head=*/nil, html_for_picture);
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, kImageSource);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that the correct src is found for elements with background-image.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageElementAtPointBackgroundImageCSS) {
  NSString* html_for_div =
      [NSString stringWithFormat:@"<div style='background-image:url(%s);%s' />",
                                 kImageSource, kBackgroundDivStyle];
  NSString* html = GetHtmlForPage(/*head=*/nil, html_for_div);
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, kImageSource);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that the correct src is found for images behind transparent layers.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageElementAtPointBehindTransparentLayer) {
  NSString* html_for_img = GetHtmlForImage(
      kImageSource, kImageAlt, /*title=*/nullptr, /*style=*/nullptr);
  NSString* html_for_div =
      [NSString stringWithFormat:@"%@<div style='%sopacity:0;' />",
                                 html_for_img, kOverlayDivStyle];
  NSString* html = GetHtmlForPage(/*head=*/nil, html_for_div);
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, kImageSource);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that opaque objects block image selection underneath.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageElementAtPointBehindOpaqueLayer) {
  NSString* html_for_img = GetHtmlForImage(
      kImageSource, kImageAlt, /*title=*/nullptr, /*style=*/nullptr);
  NSString* html_for_div =
      [NSString stringWithFormat:@"%@<div style='%sopacity:1;' />",
                                 html_for_img, kOverlayDivStyle];
  NSString* html = GetHtmlForPage(/*head=*/nil, html_for_div);
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  // Check that the paragraph was caught instead.
  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementTagName, "DIV");

  std::vector<const char*> ignored_keys;
  ignored_keys.push_back(kContextMenuElementTextOffset);

  CheckElementResult(kPointOnImage, expected_value, ignored_keys, false);
}

// Tests that the correct title is found for an image.
TEST_F(ContextMenuJsFindElementAtPointTest, FindImageElementWithTitleAtPoint) {
  const char image_title[] = "Hello world!";
  NSString* html = GetHtmlForPage(
      /*head=*/nil,
      GetHtmlForImage(kImageSource, kImageAlt, image_title, /*style=*/nullptr));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, kImageSource);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementTitle, image_title);
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that the correct natural size is found for an image.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageElementWithNaturalSizeAtPoint) {
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForImage());
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, kImageSource);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that image details are not returned for a point outside of the document
// margins.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageElementAtPointOutsideDocument) {
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForImage());
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);

  CheckElementResult(kPointOutsideDocument, expected_value, false);
}

// Tests that image details are not returned for a point outside of the element.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageElementAtPointOutsideElement) {
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForImage());
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);

  CheckElementResult(kPointOutsideImage, expected_value, false);
}

#pragma mark - Image with link

// Tests that an image link returns details for both the image and the link
// destination when the image source is a file:// url.
TEST_F(ContextMenuJsFindElementAtPointTest, FindLinkImageAtPointForFileUrl) {
  const char image_link[] = "file:///linky";
  NSString* html = GetHtmlForPage(
      /*head=*/nil, GetHtmlForLink(image_link, GetHtmlForImage()));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, kImageSource);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementHyperlink, image_link);
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that an image link does not return image and link details for a point
// outside the document.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindLinkImageAtPointOutsideDocument) {
  const char image_link[] = "file:///linky";
  NSString* html = GetHtmlForPage(
      /*head=*/nil, GetHtmlForLink(image_link, GetHtmlForImage()));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);

  CheckElementResult(kPointOutsideDocument, expected_value, false);
}

// Tests that an image link does not return image and link details for a point
// outside the element.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindLinkImageAtPointOutsideElement) {
  const char image_link[] = "file:///linky";
  NSString* html = GetHtmlForPage(
      /*head=*/nil, GetHtmlForLink(image_link, GetHtmlForImage()));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);

  CheckElementResult(kPointOutsideImage, expected_value, false);
}

// Tests that an image link returns details for both the image and the link
// destination when the image source is a relative url.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindLinkImageAtPointForRelativeUrl) {
  const char image_link[] = "http://destination/";
  const char relative_image_path[] = "relativeImage";
  NSString* html = GetHtmlForPage(
      /*head=*/nil,
      GetHtmlForLink(image_link, ImageHtmlWithSource(relative_image_path)));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  std::string image_source =
      base::StringPrintf("%s%s", kTestUrl, relative_image_path);

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, image_source);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementHyperlink, image_link);
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that an image link returns details for both the image and the link when
// the link points to JavaScript that is not a NOP.
TEST_F(ContextMenuJsFindElementAtPointTest, FindImageLinkedToJavaScript) {
  const char image_link[] = "javascript:console.log('whatever')";
  const char relative_image_path[] = "relativeImage";
  NSString* html = GetHtmlForPage(
      /*head=*/nil,
      GetHtmlForLink(image_link, ImageHtmlWithSource(relative_image_path)));

  // A page with a link with some JavaScript that does not result in a NOP.
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  std::string image_source =
      base::StringPrintf("%s%s", kTestUrl, relative_image_path);

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, image_source);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementHyperlink, image_link);
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that an image link returns details for only the image and not the link
// when the link points to NOP JavaScript.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageLinkedToNOPJavaScriptSemicolon) {
  const char image_link[] = "javascript:;";
  const char relative_image_path[] = "relativeImage";
  NSString* html = GetHtmlForPage(
      /*head=*/nil,
      GetHtmlForLink(image_link, ImageHtmlWithSource(relative_image_path)));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  std::string image_source =
      base::StringPrintf("%s%s", kTestUrl, relative_image_path);

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, image_source);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  // Make sure the returned JSON does not have an 'href' key.
  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that an image link returns details for only the image and not the link
// when the link points to NOP JavaScript.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageLinkedToNOPJavaScriptVoid) {
  const char image_link[] = "javascript:void(0);";
  const char relative_image_path[] = "relativeImage";
  NSString* html = GetHtmlForPage(
      /*head=*/nil,
      GetHtmlForLink(image_link, ImageHtmlWithSource(relative_image_path)));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  std::string image_source =
      base::StringPrintf("%s%s", kTestUrl, relative_image_path);

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, image_source);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  // Make sure the returned JSON does not have an 'href' key.
  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that an image link returns details for only the image and not the link
// when the link points to NOP JavaScript.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageLinkedToNOPJavaScriptMultipleVoid) {
  const char image_link[] = "javascript:void(0);  void(0); void(0)";
  NSString* html = GetHtmlForPage(
      /*head=*/nil, GetHtmlForLink(image_link, GetHtmlForImage()));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementSource, kImageSource);
  expected_value.SetStringKey(kContextMenuElementAlt, kImageAlt);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementTagName, "img");

  // Make sure the returned JSON does not have an 'href' key.
  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that only the parent link details are returned for an image with
// "-webkit-touch-callout:none" style and a parent link.
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfImageWithCalloutNone) {
  const char image_link[] = "http://destination/";
  NSString* image_html =
      GetHtmlForImage(kImageSource, kImageAlt, /*title=*/nullptr,
                      "-webkit-touch-callout:none;");
  NSString* html =
      GetHtmlForPage(/*head=*/nil, GetHtmlForLink(image_link, image_html));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementInnerText, "");
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  base::Value bounding_box_expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementHyperlink, image_link);
  expected_value.SetStringKey(kContextMenuElementTagName, "a");

  CheckElementResult(kPointOnImage, expected_value, false);
}

#pragma mark - SVG shape links

// Tests that an SVG shape link returns details for the link.
TEST_F(ContextMenuJsFindElementAtPointTest, FindSvgLinkAtPoint) {
  const char link[] = "file:///linky";
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForSvgLink(link));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementHyperlink, link);
  expected_value.SetStringKey(kContextMenuElementTagName, "a");

  CheckElementResult(kPointOnSvgLink, expected_value, false);
}

// Tests that an SVG shape xlink returns details for the link.
TEST_F(ContextMenuJsFindElementAtPointTest, FindSvgXlinkAtPoint) {
  const char link[] = "file:///linky";
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForSvgXlink(link));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementHyperlink, link);
  expected_value.SetStringKey(kContextMenuElementTagName, "a");

  CheckElementResult(kPointOnSvgLink, expected_value, false);
}

// Tests that a point within an SVG element but outside a linked shape does not
// return details for the link.
TEST_F(ContextMenuJsFindElementAtPointTest, FindSvgLinkAtPointOutsideElement) {
  const char link[] = "file:///linky";
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForSvgXlink(link));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  // Check that the paragraph was caught instead.
  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementTagName, "P");

  std::vector<const char*> ignored_keys;
  ignored_keys.push_back(kContextMenuElementTextOffset);

  CheckElementResult(kPointOutsideSvgLink, expected_value, ignored_keys, false);
}

#pragma mark -

// Tests that a text input field prevents returning details for an image behind
// the field.
TEST_F(ContextMenuJsFindElementAtPointTest, TextAreaStopsProximity) {
  NSString* body = GetHtmlForImage();
  // Cover the image with a text input.
  NSString* text_area = [NSString
      stringWithFormat:@"<input type='text' name='name' "
                       @"style='position:absolute;left:0px;width:0px;%s'/>",
                       kImageSizeStyle];
  body = [body stringByAppendingString:text_area];

  ASSERT_TRUE(web::test::LoadHtml(web_view_, GetHtmlForPage(/*head=*/nil, body),
                                  GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);

  CheckElementResult(kPointOnImage, expected_value, false);
}

// Tests that __gCrWeb.findElementAtPoint reports "never" as the referrer
// policy for pages that have an unsupported policy in a meta tag.
TEST_F(ContextMenuJsFindElementAtPointTest, UnsupportedReferrerPolicy) {
  // A page with an unsupported referrer meta tag and an image.
  NSString* const head =
      @"<meta name=\"referrer\" content=\"unsupported-value\">";
  NSString* html = GetHtmlForPage(head, GetHtmlForImage());

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value result = FindElementAtPoint(kPointOnImage, false);
  ASSERT_TRUE(result.is_dict());
  std::string* policy = result.FindStringKey(kContextMenuElementReferrerPolicy);
  ASSERT_TRUE(policy);
  EXPECT_STREQ("never", policy->c_str());
}

// Tests that __gCrWeb.findElementAtPoint finds an element at the bottom of a
// very long page.
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextFromTallPage) {
  // TODO(crbug.com/1219869): Fix on iOS 15 and reenable. This test appears to
  // fail flakily if the webview is not in the view hierarchy.
  if (@available(iOS 15, *)) {
    return;
  }

  const char link[] = "http://destination/";
  NSString* body = @"<div style='height:4000px'></div>";
  body = [body stringByAppendingString:GetHtmlForLink(link, @"link")];

  ASSERT_TRUE(web::test::LoadHtml(web_view_, GetHtmlForPage(/*head=*/nil, body),
                                  GetTestURL()));

  // Force layout to ensure `content_height` below is correct.
  EXPECT_TRUE(web::test::WaitForInjectedScripts(web_view_));
  web::test::ExecuteJavaScript(web_view_,
                               @"document.getElementsByTagName('p')");

  // Scroll the webView to the bottom to make the link accessible.
  CGFloat content_height = GetWebViewContentSize().height;
  // Fail the test with a clear error if the content height can not be fetched.
  ASSERT_GT(content_height, 0.0);

  CGFloat scroll_view_height = CGRectGetHeight(web_view_.scrollView.frame);
  CGFloat offset = content_height - scroll_view_height;
  web_view_.scrollView.contentOffset = CGPointMake(0.0, offset);

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementInnerText, "link");
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementHyperlink, link);
  expected_value.SetStringKey(kContextMenuElementTagName, "a");

  // Link is at bottom of the page content.
  CheckElementResult(CGPointMake(50.0, content_height - 100), expected_value,
                     false);
}

// Tests that __gCrWeb.findElementAtPoint finds a link inside shadow DOM
// content.
TEST_F(ContextMenuJsFindElementAtPointTest, ShadowDomLink) {
  const char link[] = "http://destination/";
  ASSERT_TRUE(web::test::LoadHtml(
      web_view_,
      GetHtmlForPage(/*head=*/nil, GetHtmlForShadowDomLink(link, @"link")),
      GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementInnerText, "link");
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementHyperlink, link);
  expected_value.SetStringKey(kContextMenuElementTagName, "a");

  CheckElementResult(kPointOnShadowDomLink, expected_value, false);
}

// Tests that a point within shadow DOM content but not on a link does not
// return details for the link.
TEST_F(ContextMenuJsFindElementAtPointTest, PointOutsideShadowDomLink) {
  const char link[] = "http://destination/";
  ASSERT_TRUE(web::test::LoadHtml(
      web_view_,
      GetHtmlForPage(/*head=*/nil, GetHtmlForShadowDomLink(link, @"link")),
      GetTestURL()));

  // Check that the paragraph was caught instead.
  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementTagName, "DIV");

  std::vector<const char*> ignored_keys;
  ignored_keys.push_back(kContextMenuElementTextOffset);

  CheckElementResult(kPointOutsideShadowDomLink, expected_value, ignored_keys,
                     false);
}

// Tests that a callout information about a link is displayed when
// -webkit-touch-callout property is not specified. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithoutCalloutProperty) {
  const char link[] = "http://destination/";
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForLink(link, @"link"));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementInnerText, "link");
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementHyperlink, link);
  expected_value.SetStringKey(kContextMenuElementTagName, "a");

  CheckElementResult(kPointOnLink, expected_value, false);
}

// Tests that a callout information about a link is displayed when
// -webkit-touch-callout property is set to default. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutDefault) {
  const char link[] = "http://destination/";
  const char link_style[] = "-webkit-touch-callout:default;";
  NSString* html =
      GetHtmlForPage(/*head=*/nil, GetHtmlForLink(link, "link", link_style));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementInnerText, "link");
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementHyperlink, link);
  expected_value.SetStringKey(kContextMenuElementTagName, "a");

  CheckElementResult(kPointOnLink, expected_value, false);
}

// Tests that no callout information about a link is displayed when
// -webkit-touch-callout property is set to none. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutNone) {
  const char link[] = "http://destination/";
  const char link_style[] = "-webkit-touch-callout:none;";
  NSString* html =
      GetHtmlForPage(/*head=*/nil, GetHtmlForLink(link, "link", link_style));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  // Check that the paragraph was caught instead.
  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementInnerText, "link");
  expected_value.SetStringKey(kContextMenuElementTagName, "P");

  std::vector<const char*> ignored_keys;
  ignored_keys.push_back(kContextMenuElementTextOffset);

  CheckElementResult(kPointOnLink, expected_value, ignored_keys, false);
}

// Tests that -webkit-touch-callout property can be inherited from ancester
// if it's not specified. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutFromAncester) {
  NSString* const head = @"<style>body { -webkit-touch-callout:none; }</style>";
  const char link[] = "http://destination/";
  NSString* html = GetHtmlForPage(head, GetHtmlForLink(link, @"link"));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  // Check that the paragraph was caught instead.
  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementInnerText, "link");
  expected_value.SetStringKey(kContextMenuElementTagName, "P");

  std::vector<const char*> ignored_keys;
  ignored_keys.push_back(kContextMenuElementTextOffset);

  CheckElementResult(kPointOnLink, expected_value, ignored_keys, false);
}

// Tests that setting -webkit-touch-callout property can override the value
// inherited from ancester. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutOverride) {
  NSString* head = @"<style>body { -webkit-touch-callout:none; }</style>";
  const char link[] = "http://destination/";
  const char link_style[] = "-webkit-touch-callout:default;";
  NSString* html =
      GetHtmlForPage(head, GetHtmlForLink(link, "link", link_style));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  base::Value expected_value(base::Value::Type::DICTIONARY);
  expected_value.SetStringKey(kContextMenuElementRequestId, kRequestId);
  expected_value.SetStringKey(kContextMenuElementInnerText, "link");
  expected_value.SetStringKey(kContextMenuElementReferrerPolicy, "default");
  expected_value.SetStringKey(kContextMenuElementHyperlink, link);
  expected_value.SetStringKey(kContextMenuElementTagName, "a");

  CheckElementResult(kPointOnLink, expected_value, false);
}

}  // namespace web
