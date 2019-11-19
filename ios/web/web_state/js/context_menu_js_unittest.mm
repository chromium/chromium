// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include "base/macros.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/web_state/context_menu_constants.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Unit tests for ios/web/web_state/js/resources/context_menu.js.

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// A class which handles receiving script message responses by implementing the
// WKScriptMessageHandler protocol.
@interface CRWFakeScriptMessageHandler : NSObject<WKScriptMessageHandler>
@property(nonatomic) WKScriptMessage* lastReceivedScriptMessage;
@end

@implementation CRWFakeScriptMessageHandler
@synthesize lastReceivedScriptMessage = _lastReceivedScriptMessage;

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  _lastReceivedScriptMessage = message;
}
@end

namespace {

// Request id used for __gCrWeb.findElementAtPoint call.
NSString* const kRequestId = @"UNIQUE_IDENTIFIER";

// The base url for loaded web pages.
const char kTestUrl[] = "https://chromium.test/";

// A point in the web view's coordinate space on the link returned by
// |GetHtmlForLink()|.
const CGPoint kPointOnLink = {5.0, 2.0};

// A point in the web view's coordinate space on the image returned by
// |GetHtmlForImage()|.
const CGPoint kPointOnImage = {50.0, 10.0};
// A point in the web view's coordinate space within the document bounds but not
// on the image returned by |GetHtmlForImage()|.
const CGPoint kPointOutsideImage = {50.0, 75.0};

// A point in the web view's coordinate space on the svg link returned by
// |GetHtmlForSvgLink()| and |GetHtmlForSvgXlink()|.
const CGPoint kPointOnSvgLink = {50.0, 75.0};
// A point in the web view's coordinate space within the svg element but not
// on the svg link returned by |GetHtmlForSvgLink()| and |GetHtmlForSvgXlink()|.
const CGPoint kPointOutsideSvgLink = {50.0, 10.0};

// A point in the web view's coordinate space on the shadow DOM link returned by
// |GetHtmlForShadowDomLink()|.
const CGPoint kPointOnShadowDomLink = {5.0, 2.0};
// A point in the web view's coordinate space within the shadow DOM returned by
// |GetHtmlForShadowDomLink()| but not on the link.
const CGPoint kPointOutsideShadowDomLink = {50.0, 75.0};

// A point in the web view's coordinate space outside of the document bounds.
const CGPoint kPointOutsideDocument = {150.0, 150.0};

// A base64 encoded svg image of a blue square.
NSString* const kImageSource =
    @"data:image/"
    @"svg+xml;base64,PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiI"
     "HN0YW5kYWxvbmU9InllcyI/"
     "Pgo8c3ZnIHdpZHRoPSI2MDAiIGhlaWdodD0iNjAwIiB4bWxucz0i"
     "aHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZlcnNpb249IjEuMSI+"
     "CjxyZWN0IHdpZHRoPSI"
     "2MDAiIGhlaWdodD0iNjAwIiBmaWxsPSIjMDA2NmZmIi8+Cjwvc3ZnPg==";

// Style used to size the image returned by |GetHtmlForImage()|.
NSString* const kImageSizeStyle = @"width:100%;height:25%;";

// Returns HTML for a test webpage with the given |head| and |body|.
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

// Returns HTML for a link to |href|, display |text|, and inline |style|.
NSString* GetHtmlForLink(NSString* href, NSString* text, NSString* style) {
  NSString* style_attribute =
      style ? [NSString stringWithFormat:@"style=\"%@\" ", style] : @"";
  return [NSString
      stringWithFormat:@"<a %@href=\"%@\">%@</a>", style_attribute, href, text];
}

// Returns HTML for an SVG shape which links to |href|.
NSString* GetHtmlForSvgLink(NSString* href) {
  NSString* svg_shape = @"<rect y=\"50\" width=\"100\" height=\"50\"/>";
  return [NSString
      stringWithFormat:
          @"<svg width=\"100\" height=\"100\"><a href=\"%@\">%@</a></svg>",
          href, svg_shape];
}

// Returns HTML for an SVG shape which links to |href| with an xlink:href.
NSString* GetHtmlForSvgXlink(NSString* href) {
  NSString* svg_shape = @"<rect y=\"50\" width=\"100\" height=\"50\"/>";
  return [NSString stringWithFormat:@"<svg width=\"100\" height=\"100\"><a "
                                    @"xlink:href=\"%@\">%@</a></svg>",
                                    href, svg_shape];
}

// Returns HTML for a link to |href| and display text |text|.
NSString* GetHtmlForLink(NSString* href, NSString* text) {
  return GetHtmlForLink(href, text, /*style=*/nil);
}

// Returns HTML for a shadow DOM link to |href| and display text |text|.
NSString* GetHtmlForShadowDomLink(NSString* href, NSString* text) {
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
// container. |source| must be provided, but specifying an image |title| and
// inline |style| are optional.
NSString* GetHtmlForImage(NSString* source, NSString* title, NSString* style) {
  NSString* additional_css = style ? style : @"";
  NSString* image_title =
      title ? [NSString stringWithFormat:@"title='%@' ", title] : @"";
  return [NSString
      stringWithFormat:@"<img id='image' %@style='%@%@' src='%@'/>",
                       image_title, kImageSizeStyle, additional_css, source];
}

// Returns html for an image styled to fill the width and top 25% of its
// container.
NSString* GetHtmlForImage() {
  return GetHtmlForImage(kImageSource, /*title=*/nil, /*style=*/nil);
}

// Returns html for an image styled to fill the width and top 25% of its
// container.
NSString* ImageHtmlWithSource(NSString* source) {
  return GetHtmlForImage(source, /*title=*/nil, /*style=*/nil);
}

}  // namespace

namespace web {

// Test fixture to test __gCrWeb.findElementAtPoint function defined in
// context_menu.js.
class ContextMenuJsFindElementAtPointTest : public web::WebTest {
 public:
  ContextMenuJsFindElementAtPointTest()
      : script_message_handler_([[CRWFakeScriptMessageHandler alloc] init]),
        web_view_(web::BuildWKWebView(CGRectMake(0.0, 0.0, 100.0, 100.0),
                                      GetBrowserState())) {
    [web_view_.configuration.userContentController
        addScriptMessageHandler:script_message_handler_
                           name:@"FindElementResultHandler"];
  }

 protected:
  // Returns details of the DOM element at the given |point| in the web view
  // viewport's coordinate space.
  id FindElementAtPoint(CGPoint point) {
    EXPECT_TRUE(web::test::WaitForInjectedScripts(web_view_));

    // Force layout
    web::test::ExecuteJavaScript(web_view_,
                                 @"document.getElementsByTagName('p')");

    // Clear previous script message response.
    script_message_handler_.lastReceivedScriptMessage = nil;

    ExecuteFindElementFromPointJavaScript(point);

    // Wait for response.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return !!script_message_handler_.lastReceivedScriptMessage;
    }));

    return script_message_handler_.lastReceivedScriptMessage.body;
  }

  // Returns web view's content size from the current web state.
  CGSize GetWebViewContentSize() { return web_view_.scrollView.contentSize; }

  // Returns the test page URL.
  NSURL* GetTestURL() { return net::NSURLWithGURL(GURL(kTestUrl)); }

  // Executes __gCrWeb.findElementAtPoint script with the given |point| in the
  // web view viewport's coordinate space.
  id ExecuteFindElementFromPointJavaScript(CGPoint point) {
    CGSize size = GetWebViewContentSize();
    NSString* const script = [NSString
        stringWithFormat:@"__gCrWeb.findElementAtPoint('%@', %g, %g, %g, %g)",
                         kRequestId, point.x, point.y, size.width, size.height];
    return web::test::ExecuteJavaScript(web_view_, script);
  }

  // Handles script message responses sent from |web_view_|.
  CRWFakeScriptMessageHandler* script_message_handler_;

  // The web view used for testing.
  WKWebView* web_view_;
};

#pragma mark - Image without link

// Tests that the correct src and referrer are found for an image.
TEST_F(ContextMenuJsFindElementAtPointTest, FindImageElementAtPoint) {
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForImage());
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : kImageSource,
    kContextMenuElementReferrerPolicy : @"default",
  };

  EXPECT_NSEQ(expected_value, result);
}

// Tests that the correct title is found for an image.
TEST_F(ContextMenuJsFindElementAtPointTest, FindImageElementWithTitleAtPoint) {
  NSString* const image_title = @"Hello world!";
  NSString* html = GetHtmlForPage(
      /*head=*/nil, GetHtmlForImage(kImageSource, image_title, /*style=*/nil));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : kImageSource,
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementTitle : image_title,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that image details are not returned for a point outside of the document
// margins.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageElementAtPointOutsideDocument) {
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForImage());
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOutsideDocument);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that image details are not returned for a point outside of the element.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageElementAtPointOutsideElement) {
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForImage());
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOutsideImage);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

#pragma mark - Image with link

// Tests that an image link returns details for both the image and the link
// destination when the image source is a file:// url.
TEST_F(ContextMenuJsFindElementAtPointTest, FindLinkImageAtPointForFileUrl) {
  NSString* const image_link = @"file:///linky";
  NSString* html = GetHtmlForPage(
      /*head=*/nil, GetHtmlForLink(image_link, GetHtmlForImage()));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : kImageSource,
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : image_link,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that an image link does not return image and link details for a point
// outside the document.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindLinkImageAtPointOutsideDocument) {
  NSString* const image_link = @"file:///linky";
  NSString* html = GetHtmlForPage(
      /*head=*/nil, GetHtmlForLink(image_link, GetHtmlForImage()));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOutsideDocument);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that an image link does not return image and link details for a point
// outside the element.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindLinkImageAtPointOutsideElement) {
  NSString* const image_link = @"file:///linky";
  NSString* html = GetHtmlForPage(
      /*head=*/nil, GetHtmlForLink(image_link, GetHtmlForImage()));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOutsideImage);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that an image link returns details for both the image and the link
// destination when the image source is a relative url.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindLinkImageAtPointForRelativeUrl) {
  NSString* const image_link = @"http://destination/";
  NSString* const relative_image_path = @"relativeImage";
  NSString* html = GetHtmlForPage(
      /*head=*/nil,
      GetHtmlForLink(image_link, ImageHtmlWithSource(relative_image_path)));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource :
        [NSString stringWithFormat:@"%s%@", kTestUrl, relative_image_path],
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : image_link,
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that an image link returns details for both the image and the link when
// the link points to JavaScript that is not a NOP.
TEST_F(ContextMenuJsFindElementAtPointTest, FindImageLinkedToJavaScript) {
  NSString* const image_link = @"javascript:console.log('whatever')";
  NSString* const relative_image_path = @"relativeImage";
  NSString* html = GetHtmlForPage(
      /*head=*/nil,
      GetHtmlForLink(image_link, ImageHtmlWithSource(relative_image_path)));

  // A page with a link with some JavaScript that does not result in a NOP.
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource :
        [NSString stringWithFormat:@"%s%@", kTestUrl, relative_image_path],
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : image_link,
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that an image link returns details for only the image and not the link
// when the link points to NOP JavaScript.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageLinkedToNOPJavaScriptSemicolon) {
  NSString* const image_link = @"javascript:;";
  NSString* const relative_image_path = @"relativeImage";
  NSString* html = GetHtmlForPage(
      /*head=*/nil,
      GetHtmlForLink(image_link, ImageHtmlWithSource(relative_image_path)));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource :
        [NSString stringWithFormat:@"%s%@", kTestUrl, relative_image_path],
    kContextMenuElementReferrerPolicy : @"default",
  };
  // Make sure the returned JSON does not have an 'href' key.
  EXPECT_NSEQ(expected_result, result);
}

// Tests that an image link returns details for only the image and not the link
// when the link points to NOP JavaScript.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageLinkedToNOPJavaScriptVoid) {
  NSString* const image_link = @"javascript:void(0);";
  NSString* const relative_image_path = @"relativeImage";
  NSString* html = GetHtmlForPage(
      /*head=*/nil,
      GetHtmlForLink(image_link, ImageHtmlWithSource(relative_image_path)));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource :
        [NSString stringWithFormat:@"%s%@", kTestUrl, relative_image_path],
    kContextMenuElementReferrerPolicy : @"default",
  };
  // Make sure the returned JSON does not have an 'href' key.
  EXPECT_NSEQ(expected_result, result);
}

// Tests that an image link returns details for only the image and not the link
// when the link points to NOP JavaScript.
TEST_F(ContextMenuJsFindElementAtPointTest,
       FindImageLinkedToNOPJavaScriptMultipleVoid) {
  NSString* const image_link = @"javascript:void(0);  void(0); void(0)";
  NSString* html = GetHtmlForPage(
      /*head=*/nil, GetHtmlForLink(image_link, GetHtmlForImage()));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementSource : kImageSource,
    kContextMenuElementReferrerPolicy : @"default",
  };
  // Make sure the returned JSON does not have an 'href' key.
  EXPECT_NSEQ(expected_result, result);
}

// Tests that only the parent link details are returned for an image with
// "-webkit-touch-callout:none" style and a parent link.
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfImageWithCalloutNone) {
  NSString* const image_link = @"http://destination/";
  NSString* image_html = GetHtmlForImage(kImageSource, /*title=*/nil,
                                         @"-webkit-touch-callout:none;");
  NSString* html =
      GetHtmlForPage(/*head=*/nil, GetHtmlForLink(image_link, image_html));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : image_link,
  };
  EXPECT_NSEQ(expected_result, result);
}

#pragma mark - SVG shape links

// Tests that an SVG shape link returns details for the link.
TEST_F(ContextMenuJsFindElementAtPointTest, FindSvgLinkAtPoint) {
  NSString* const link = @"file:///linky";
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForSvgLink(link));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnSvgLink);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : link,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that an SVG shape xlink returns details for the link.
TEST_F(ContextMenuJsFindElementAtPointTest, FindSvgXlinkAtPoint) {
  NSString* const link = @"file:///linky";
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForSvgXlink(link));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnSvgLink);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : link,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that a point within an SVG element but outside a linked shape does not
// return details for the link.
TEST_F(ContextMenuJsFindElementAtPointTest, FindSvgLinkAtPointOutsideElement) {
  NSString* const link = @"file:///linky";
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForSvgXlink(link));
  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOutsideSvgLink);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

#pragma mark -

// Tests that a text input field prevents returning details for an image behind
// the field.
TEST_F(ContextMenuJsFindElementAtPointTest, TextAreaStopsProximity) {
  NSString* body = GetHtmlForImage();
  // Cover the image with a text input.
  NSString* text_area =
      [NSString stringWithFormat:
                    @"<input type='text' name='name' "
                    @"style='position:absolute;left:0px;width:0px;%@'/>",
                    kImageSizeStyle];
  body = [body stringByAppendingString:text_area];

  ASSERT_TRUE(web::test::LoadHtml(web_view_, GetHtmlForPage(/*head=*/nil, body),
                                  GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  NSDictionary* expected_value = @{
    kContextMenuElementRequestId : kRequestId,
  };
  EXPECT_NSEQ(expected_value, result);
}

// Tests that __gCrWeb.findElementAtPoint reports "never" as the referrer
// policy for pages that have an unsupported policy in a meta tag.
TEST_F(ContextMenuJsFindElementAtPointTest, UnsupportedReferrerPolicy) {
  // A page with an unsupported referrer meta tag and an image.
  NSString* const head =
      @"<meta name=\"referrer\" content=\"unsupported-value\">";
  NSString* html = GetHtmlForPage(head, GetHtmlForImage());

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnImage);
  ASSERT_TRUE([result isKindOfClass:[NSDictionary class]]);
  EXPECT_NSEQ(@"never", result[kContextMenuElementReferrerPolicy]);
}

// Tests that __gCrWeb.findElementAtPoint finds an element at the bottom of a
// very long page.
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextFromTallPage) {
  NSString* const link = @"http://destination/";
  NSString* body = @"<div style='height:4000px'></div>";
  body = [body stringByAppendingString:GetHtmlForLink(link, @"link")];

  ASSERT_TRUE(web::test::LoadHtml(web_view_, GetHtmlForPage(/*head=*/nil, body),
                                  GetTestURL()));

  // Force layout to ensure |content_height| below is correct.
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

  // Link is at bottom of the page content.
  id result = FindElementAtPoint(CGPointMake(50.0, content_height - 100));

  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : link,
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that __gCrWeb.findElementAtPoint finds a link inside shadow DOM
// content.
TEST_F(ContextMenuJsFindElementAtPointTest, ShadowDomLink) {
  NSString* const link = @"http://destination/";
  ASSERT_TRUE(web::test::LoadHtml(
      web_view_,
      GetHtmlForPage(/*head=*/nil, GetHtmlForShadowDomLink(link, @"link")),
      GetTestURL()));

  id result = FindElementAtPoint(kPointOnShadowDomLink);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : link,
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that a point within shadow DOM content but not on a link does not
// return details for the link.
TEST_F(ContextMenuJsFindElementAtPointTest, PointOutsideShadowDomLink) {
  NSString* const link = @"http://destination/";
  ASSERT_TRUE(web::test::LoadHtml(
      web_view_,
      GetHtmlForPage(/*head=*/nil, GetHtmlForShadowDomLink(link, @"link")),
      GetTestURL()));

  id result = FindElementAtPoint(kPointOutsideShadowDomLink);
  EXPECT_NSEQ(@{kContextMenuElementRequestId : kRequestId}, result);
}

// Tests that a callout information about a link is displayed when
// -webkit-touch-callout property is not specified. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithoutCalloutProperty) {
  NSString* const link = @"http://destination/";
  NSString* html = GetHtmlForPage(/*head=*/nil, GetHtmlForLink(link, @"link"));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnLink);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : link,
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that a callout information about a link is displayed when
// -webkit-touch-callout property is set to default. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutDefault) {
  NSString* const link = @"http://destination/";
  NSString* const link_style = @"-webkit-touch-callout:default;";
  NSString* html =
      GetHtmlForPage(/*head=*/nil, GetHtmlForLink(link, @"link", link_style));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnLink);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : link,
  };
  EXPECT_NSEQ(expected_result, result);
}

// Tests that no callout information about a link is displayed when
// -webkit-touch-callout property is set to none. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutNone) {
  NSString* const link = @"http://destination/";
  NSString* const link_style = @"-webkit-touch-callout:none;";
  NSString* html =
      GetHtmlForPage(/*head=*/nil, GetHtmlForLink(link, @"link", link_style));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnLink);
  EXPECT_NSEQ(@{kContextMenuElementRequestId : kRequestId}, result);
}

// Tests that -webkit-touch-callout property can be inherited from ancester
// if it's not specified. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutFromAncester) {
  NSString* const head = @"<style>body { -webkit-touch-callout:none; }</style>";
  NSString* const link = @"http://destination/";
  NSString* html = GetHtmlForPage(head, GetHtmlForLink(link, @"link"));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnLink);
  EXPECT_NSEQ(@{kContextMenuElementRequestId : kRequestId}, result);
}

// Tests that setting -webkit-touch-callout property can override the value
// inherited from ancester. Please see:
// https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-touch-callout
TEST_F(ContextMenuJsFindElementAtPointTest, LinkOfTextWithCalloutOverride) {
  NSString* const head = @"<style>body { -webkit-touch-callout:none; }</style>";
  NSString* const link = @"http://destination/";
  NSString* const link_style = @"-webkit-touch-callout:default;";
  NSString* html =
      GetHtmlForPage(head, GetHtmlForLink(link, @"link", link_style));

  ASSERT_TRUE(web::test::LoadHtml(web_view_, html, GetTestURL()));

  id result = FindElementAtPoint(kPointOnLink);
  NSDictionary* expected_result = @{
    kContextMenuElementRequestId : kRequestId,
    kContextMenuElementInnerText : @"link",
    kContextMenuElementReferrerPolicy : @"default",
    kContextMenuElementHyperlink : link,
  };
  EXPECT_NSEQ(expected_result, result);
}

}  // namespace web
