// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"

#import <WebKit/WebKit.h>

#include "base/ios/ios_util.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios_private.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_view_creation_util.h"
#include "net/base/load_flags.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Whether the iOS specialization of the GaiaAuthFetcher should be used.
bool g_should_use_gaia_auth_fetcher_ios = true;

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

// Creates an NSURLRequest to |url| that can be loaded by a WebView from |body|
// and |headers|.
// The request is a GET if |body| is empty and a POST otherwise.
NSURLRequest* GetRequest(const std::string& body,
                         const std::string& headers,
                         const GURL& url) {
  NSMutableURLRequest* request =
      [[NSMutableURLRequest alloc] initWithURL:net::NSURLWithGURL(url)];
  net::HttpRequestHeaders request_headers;
  request_headers.AddHeadersFromString(headers);
  for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();) {
    [request setValue:base::SysUTF8ToNSString(it.value())
        forHTTPHeaderField:base::SysUTF8ToNSString(it.name())];
  }
  if (!body.empty()) {
    NSData* post_data =
        [base::SysUTF8ToNSString(body) dataUsingEncoding:NSUTF8StringEncoding];
    [request setHTTPBody:post_data];
    [request setHTTPMethod:@"POST"];
    DCHECK(![[request allHTTPHeaderFields] objectForKey:@"Content-Type"]);
    [request setValue:@"application/x-www-form-urlencoded"
        forHTTPHeaderField:@"Content-Type"];
  }
  return request;
}

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

@implementation GaiaAuthFetcherNavigationDelegate {
  GaiaAuthFetcherIOSBridge* bridge_;  // weak
}
- (instancetype)initWithBridge:(GaiaAuthFetcherIOSBridge*)bridge {
  self = [super init];
  if (self) {
    bridge_ = bridge;
  }
  return self;
}

#pragma mark WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  DVLOG(1) << "Gaia fetcher navigation failed: "
           << base::SysNSStringToUTF8(error.localizedDescription);
  bridge_->URLFetchFailure(false /* is_cancelled */);
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  DVLOG(1) << "Gaia fetcher provisional navigation failed: "
           << base::SysNSStringToUTF8(error.localizedDescription);
  bridge_->URLFetchFailure(false /* is_cancelled */);
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  // A WKNavigation is an opaque object. The only way to access the body of the
  // response is via Javascript.
  DVLOG(2) << "WKWebView loaded:" << net::GURLWithNSURL(webView.URL);
  [webView evaluateJavaScript:kReadResponseTemplate
            completionHandler:^(NSString* result, NSError* error) {
              if (error || !result) {
                DVLOG(1) << "Gaia fetcher extract body failed:"
                         << base::SysNSStringToUTF8(error.localizedDescription);
                bridge_->URLFetchFailure(false /* is_cancelled */);
              } else {
                DCHECK([result isKindOfClass:[NSString class]]);
                bridge_->URLFetchSuccess(base::SysNSStringToUTF8(result));
              }
            }];
}

@end

#pragma mark - GaiaAuthFetcherIOSBridge::Request

GaiaAuthFetcherIOSBridge::Request::Request()
    : pending(false),
      url(),
      headers(),
      body(),
      shouldUseXmlHTTPRequest(false) {}

GaiaAuthFetcherIOSBridge::Request::Request(const GURL& request_url,
                                           const std::string& request_headers,
                                           const std::string& request_body,
                                           bool shouldUseXmlHTTPRequest)
    : pending(true),
      url(request_url),
      headers(request_headers),
      body(request_body),
      shouldUseXmlHTTPRequest(shouldUseXmlHTTPRequest) {}

#pragma mark - GaiaAuthFetcherIOSBridge

GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridge(
    GaiaAuthFetcherIOS* fetcher,
    web::BrowserState* browser_state)
    : browser_state_(browser_state), fetcher_(fetcher), request_() {
  ActiveStateManager::FromBrowserState(browser_state_)->AddObserver(this);
}

GaiaAuthFetcherIOSBridge::~GaiaAuthFetcherIOSBridge() {
  ActiveStateManager::FromBrowserState(browser_state_)->RemoveObserver(this);
  ResetWKWebView();
}

void GaiaAuthFetcherIOSBridge::Fetch(const GURL& url,
                                     const std::string& headers,
                                     const std::string& body,
                                     bool shouldUseXmlHTTPRequest) {
  request_ = Request(url, headers, body, shouldUseXmlHTTPRequest);
  FetchPendingRequest();
}

void GaiaAuthFetcherIOSBridge::Cancel() {
  if (!request_.pending) {
    return;
  }
  [GetWKWebView() stopLoading];
  URLFetchFailure(true /* is_cancelled */);
}

void GaiaAuthFetcherIOSBridge::URLFetchSuccess(const std::string& data) {
  if (!request_.pending) {
    return;
  }
  GURL url = FinishPendingRequest();
  // WKWebViewNavigationDelegate API doesn't give any way to get the HTTP
  // response code of a navigation. Default to 200 for success.
  fetcher_->FetchComplete(url, data, net::ResponseCookies(),
                          net::URLRequestStatus(), 200);
}

void GaiaAuthFetcherIOSBridge::URLFetchFailure(bool is_cancelled) {
  if (!request_.pending) {
    return;
  }
  GURL url = FinishPendingRequest();
  // WKWebViewNavigationDelegate API doesn't give any way to get the HTTP
  // response code of a navigation. Default to 500 for error.
  int error = is_cancelled ? net::ERR_ABORTED : net::ERR_FAILED;
  fetcher_->FetchComplete(url, std::string(), net::ResponseCookies(),
                          net::URLRequestStatus::FromError(error), 500);
}

void GaiaAuthFetcherIOSBridge::FetchPendingRequest() {
  if (!request_.pending)
    return;
  if (!request_.body.empty() && request_.shouldUseXmlHTTPRequest) {
    DoPostRequest(GetWKWebView(), request_.body, request_.headers,
                  request_.url);
  } else {
    [GetWKWebView()
        loadRequest:GetRequest(request_.body, request_.headers, request_.url)];
  }
}

GURL GaiaAuthFetcherIOSBridge::FinishPendingRequest() {
  GURL url = request_.url;
  request_ = Request();
  return url;
}

WKWebView* GaiaAuthFetcherIOSBridge::GetWKWebView() {
  if (!ActiveStateManager::FromBrowserState(browser_state_)->IsActive()) {
    // |browser_state_| is not active, WKWebView linked to this browser state
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

void GaiaAuthFetcherIOSBridge::ResetWKWebView() {
  [web_view_ setNavigationDelegate:nil];
  [web_view_ stopLoading];
  web_view_ = nil;
  navigation_delegate_ = nil;
}

WKWebView* GaiaAuthFetcherIOSBridge::BuildWKWebView() {
  return web::BuildWKWebView(CGRectZero, browser_state_);
}

void GaiaAuthFetcherIOSBridge::OnActive() {
  // |browser_state_| is now active. If there is a pending request, restart it.
  FetchPendingRequest();
}

void GaiaAuthFetcherIOSBridge::OnInactive() {
  // |browser_state_| is now inactive. Stop using |web_view_| and don't create
  // a new one until it is active.
  ResetWKWebView();
}

#pragma mark - GaiaAuthFetcherIOS definition

GaiaAuthFetcherIOS::GaiaAuthFetcherIOS(
    GaiaAuthConsumer* consumer,
    const std::string& source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    web::BrowserState* browser_state)
    : GaiaAuthFetcher(consumer, source, url_loader_factory),
      bridge_(new GaiaAuthFetcherIOSBridge(this, browser_state)),
      browser_state_(browser_state) {}

GaiaAuthFetcherIOS::~GaiaAuthFetcherIOS() {
}

void GaiaAuthFetcherIOS::CreateAndStartGaiaFetcher(
    const std::string& body,
    const std::string& headers,
    const GURL& gaia_gurl,
    int load_flags,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!HasPendingFetch()) << "Tried to fetch two things at once!";

  bool cookies_required = !(load_flags & (net::LOAD_DO_NOT_SEND_COOKIES |
                                          net::LOAD_DO_NOT_SAVE_COOKIES));
  if (!ShouldUseGaiaAuthFetcherIOS() || !cookies_required) {
    GaiaAuthFetcher::CreateAndStartGaiaFetcher(body, headers, gaia_gurl,
                                               load_flags, traffic_annotation);
    return;
  }

  DVLOG(2) << "Gaia fetcher URL: " << gaia_gurl.spec();
  DVLOG(2) << "Gaia fetcher headers: " << headers;
  DVLOG(2) << "Gaia fetcher body: " << body;

  // The fetch requires cookies and WKWebView is being used. The only way to do
  // a network request with cookies sent and saved is by making it through a
  // WKWebView.
  SetPendingFetch(true);
  bool shouldUseXmlHTTPRequest =
      IsMultiloginUrl(gaia_gurl) || !base::ios::IsRunningOnIOS11OrLater();
  bridge_->Fetch(gaia_gurl, headers, body, shouldUseXmlHTTPRequest);
}

void GaiaAuthFetcherIOS::CancelRequest() {
  if (!HasPendingFetch()) {
    return;
  }
  bridge_->Cancel();
  GaiaAuthFetcher::CancelRequest();
}

void GaiaAuthFetcherIOS::FetchComplete(const GURL& url,
                                       const std::string& data,
                                       const net::ResponseCookies& cookies,
                                       const net::URLRequestStatus& status,
                                       int response_code) {
  DVLOG(2) << "Response " << url.spec() << ", code = " << response_code << "\n";
  DVLOG(2) << "data: " << data << "\n";
  SetPendingFetch(false);
  DispatchFetchedRequest(url, data, cookies,
                         static_cast<net::Error>(status.error()),
                         response_code);
}

void GaiaAuthFetcherIOS::SetShouldUseGaiaAuthFetcherIOSForTesting(
    bool use_gaia_fetcher_ios) {
  g_should_use_gaia_auth_fetcher_ios = use_gaia_fetcher_ios;
}

bool GaiaAuthFetcherIOS::ShouldUseGaiaAuthFetcherIOS() {
  return g_should_use_gaia_auth_fetcher_ios;
}
