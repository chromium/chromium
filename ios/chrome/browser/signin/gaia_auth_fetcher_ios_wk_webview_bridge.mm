// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/gaia_auth_fetcher_ios_wk_webview_bridge.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include "base/json/string_escape.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/common/web_view_creation_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/http/http_request_headers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// JavaScript template to do a POST request using an XMLHttpRequest.
// The request is retried once on failure, as it can be marked as failing to
// load the resource because of 302s on POST request (the cookies of the first
// response are correctly set).
//
// The template takes three arguments (in order):
// * The quoted and escaped URL to send a POST request to.
// * The HTTP headers of the request. They should be written as valid JavaScript
//   statements, adding headers to the XMLHttpRequest variable named 'req'
//   (e.g. 'req.setRequestHeader("Foo", "Bar");').
// * The quoted and escaped body of the POST request.
NSString* const kPostRequestTemplate =
    @"<html><script>"
     "function __gCrWebDoPostRequest() {"
     "  function createAndSendPostRequest() {"
     "    var req = new XMLHttpRequest();"
     "    req.open(\"POST\", %@, false);"
     "    req.setRequestHeader(\"Content-Type\","
     "\"application/x-www-form-urlencoded\");"
     "%@"
     "    req.send(%@);"
     "    if (req.status != 200) {"
     "      throw req.status;"
     "    }"
     "    return req.responseText;"
     "  }"
     "  try {"
     "    return createAndSendPostRequest();"
     "  } catch(err) {"
     "    return createAndSendPostRequest();"
     "  }"
     "}"
     "</script></html>";

// JavaScript template to read the response to a GET or POST request. There is
// two different cases:
// * GET request, which was made by simply loading a request to the correct
//   URL. The response is the inner text (to avoid formatting in case of JSON
//   answers) of the body.
// * POST request, in case the "__gCrWebDoPostRequest" function is defined.
//   Running the function will do a POST request via a XMLHttpRequest and
//   return the response. See DoPostRequest below to know why this is necessary.
NSString* const kReadResponseTemplate =
    @"if (typeof __gCrWebDoPostRequest === 'function') {"
     "  __gCrWebDoPostRequest();"
     "} else {"
     "  document.body.innerText;"
     "}";

// Escapes and quotes |value| and converts the result to an NSString.
NSString* EscapeAndQuoteToNSString(const std::string& value) {
  return base::SysUTF8ToNSString(base::GetQuotedJSONString(value));
}

// Simulates a POST request on |web_view| using a XMLHttpRequest in
// JavaScript.
// This is needed because WKWebView ignores the HTTPBody in a POST request
// before iOS11 and because WKWebView cannot read response body if
// content-disposition header is set. See
// https://bugs.webkit.org/show_bug.cgi?id=145410
// TODO(crbug.com/889471) Remove this once requests are done using
// NSUrlSession in iOS.
void DoPostRequest(WKWebView* web_view,
                   const std::string& body,
                   const std::string& headers,
                   const GURL& url) {
  NSMutableString* header_data = [NSMutableString string];
  net::HttpRequestHeaders request_headers;
  request_headers.AddHeadersFromString(headers);
  for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();) {
    if (it.name() == "Origin") {
      // The Origin request header cannot be set on an XMLHttpRequest.
      continue;
    }
    // net::HttpRequestHeaders escapes the name and value for a header. Some
    // escaping might still be necessary for the JavaScript layer.
    [header_data appendFormat:@"req.setRequestHeader(%@, %@);",
                              EscapeAndQuoteToNSString(it.name()),
                              EscapeAndQuoteToNSString(it.value())];
  }
  NSString* html_string =
      [NSString stringWithFormat:kPostRequestTemplate,
                                 EscapeAndQuoteToNSString(url.spec()),
                                 header_data, EscapeAndQuoteToNSString(body)];
  // |url| is used as the baseURL to avoid CORS issues.
  [web_view loadHTMLString:html_string baseURL:net::NSURLWithGURL(url)];
}
}  // namespace

#pragma mark - GaiaAuthFetcherNavigationDelegate

// Navigation delegate attached to a WKWebView used for URL fetches.
@interface GaiaAuthFetcherNavigationDelegate : NSObject <WKNavigationDelegate>

@property(nonatomic, assign) GaiaAuthFetcherIOSWKWebViewBridge* bridge;

@end

@implementation GaiaAuthFetcherNavigationDelegate

- (instancetype)initWithBridge:(GaiaAuthFetcherIOSWKWebViewBridge*)bridge {
  self = [super init];
  if (self) {
    _bridge = bridge;
  }
  return self;
}

- (void)javascriptCompletionWithResult:(NSString*)result error:(NSError*)error {
  if (error || !result) {
    DVLOG(1) << "Gaia fetcher extract body failed:"
             << base::SysNSStringToUTF8(error.localizedDescription);
    // WKWebViewNavigationDelegate API doesn't give any way to get the HTTP
    // response code of a navigation. Default to 500 for error.
    self.bridge->OnURLFetchFailure(net::ERR_FAILED, 500);
  } else {
    DCHECK([result isKindOfClass:[NSString class]]);
    // WKWebViewNavigationDelegate API doesn't give any way to get
    // the HTTP response code of a navigation. Default to 200 for
    // success.
    self.bridge->OnURLFetchSuccess(base::SysNSStringToUTF8(result), 200);
  }
}

#pragma mark WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  DVLOG(1) << "Gaia fetcher navigation failed: "
           << base::SysNSStringToUTF8(error.localizedDescription);
  self.bridge->OnURLFetchFailure(net::ERR_FAILED, 500);
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  DVLOG(1) << "Gaia fetcher provisional navigation failed: "
           << base::SysNSStringToUTF8(error.localizedDescription);
  self.bridge->OnURLFetchFailure(net::ERR_FAILED, 500);
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  // A WKNavigation is an opaque object. The only way to access the body of the
  // response is via Javascript.
  DVLOG(2) << "WKWebView loaded:" << net::GURLWithNSURL(webView.URL);
  __weak __typeof(self) weakSelf = self;
  [webView evaluateJavaScript:kReadResponseTemplate
            completionHandler:^(NSString* result, NSError* error) {
              [weakSelf javascriptCompletionWithResult:result error:error];
            }];
}

@end

#pragma mark - GaiaAuthFetcherIOSWKWebViewBridge

GaiaAuthFetcherIOSWKWebViewBridge::GaiaAuthFetcherIOSWKWebViewBridge(
    GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate* delegate,
    web::BrowserState* browser_state)
    : GaiaAuthFetcherIOSBridge(delegate, browser_state) {
  ActiveStateManager::FromBrowserState(GetBrowserState())->AddObserver(this);
}

GaiaAuthFetcherIOSWKWebViewBridge::~GaiaAuthFetcherIOSWKWebViewBridge() {
  ActiveStateManager::FromBrowserState(GetBrowserState())->RemoveObserver(this);
  ResetWKWebView();
}

void GaiaAuthFetcherIOSWKWebViewBridge::Cancel() {
  if (!GetRequest().pending)
    return;
  [GetWKWebView() stopLoading];
  OnURLFetchFailure(net::ERR_ABORTED, 500);
}

void GaiaAuthFetcherIOSWKWebViewBridge::FetchPendingRequest() {
  if (!GetRequest().pending)
    return;
  if (!GetRequest().body.empty() && GetRequest().should_use_xml_http_request) {
    DoPostRequest(GetWKWebView(), GetRequest().body, GetRequest().headers,
                  GetRequest().url);
  } else {
    [GetWKWebView() loadRequest:GetNSURLRequest()];
  }
}

WKWebView* GaiaAuthFetcherIOSWKWebViewBridge::GetWKWebView() {
  if (!ActiveStateManager::FromBrowserState(GetBrowserState())->IsActive()) {
    // |GetBrowserState()| is not active, WKWebView linked to this browser state
    // should not exist or be created.
    return nil;
  }
  if (!web_view_) {
    web_view_ = BuildWKWebView();
    navigation_delegate_ =
        [[GaiaAuthFetcherNavigationDelegate alloc] initWithBridge:this];
    [web_view_ setNavigationDelegate:navigation_delegate_];
  }
  return web_view_;
}

void GaiaAuthFetcherIOSWKWebViewBridge::ResetWKWebView() {
  [web_view_ setNavigationDelegate:nil];
  [web_view_ stopLoading];
  web_view_ = nil;
  navigation_delegate_ = nil;
}

WKWebView* GaiaAuthFetcherIOSWKWebViewBridge::BuildWKWebView() {
  return web::BuildWKWebView(CGRectZero, GetBrowserState());
}

void GaiaAuthFetcherIOSWKWebViewBridge::OnActive() {
  // |GetBrowserState()| is now active. If there is a pending request, restart
  // it.
  FetchPendingRequest();
}

void GaiaAuthFetcherIOSWKWebViewBridge::OnInactive() {
  // |GetBrowserState()| is now inactive. Stop using |web_view_| and don't
  // create a new one until it is active.
  ResetWKWebView();
}
