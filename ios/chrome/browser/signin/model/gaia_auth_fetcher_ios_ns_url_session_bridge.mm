// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/gaia_auth_fetcher_ios_ns_url_session_bridge.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/core/browser/chrome_connected_header_helper.h"
#import "ios/net/cookies/system_cookie_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_client.h"
#import "net/base/apple/url_conversions.h"
#import "net/http/http_request_headers.h"

#pragma mark - GaiaAuthFetcherIOSNSURLSessionBridge::Request

GaiaAuthFetcherIOSNSURLSessionBridge::Request::Request()
    : pending(false), should_use_xml_http_request(false) {}

GaiaAuthFetcherIOSNSURLSessionBridge::Request::Request(
    const GURL& request_url,
    const std::string& request_headers,
    const std::string& request_body,
    bool should_use_xml_http_request)
    : pending(true),
      url(request_url),
      headers(request_headers),
      body(request_body),
      should_use_xml_http_request(should_use_xml_http_request) {}

#pragma mark - GaiaAuthFetcherIOSURLSessionDelegate

@interface GaiaAuthFetcherIOSURLSessionDelegate
    : NSObject <NSURLSessionTaskDelegate>

// Gaia auth fetcher bridge.
@property(nonatomic, assign) GaiaAuthFetcherIOSNSURLSessionBridge* bridge;
// Session for the multilogin request.
@property(nonatomic, strong) NSURLSession* requestSession;

@end

@implementation GaiaAuthFetcherIOSURLSessionDelegate

@synthesize bridge = _bridge;
@synthesize requestSession = _requestSession;

#pragma mark - NSURLSessionTaskDelegate

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler {
  // If there is a redirect, the cookies from the redirect need to be stored.
  DCHECK(self.requestSession == session);
  if (self.bridge) {
    self.bridge->SetCanonicalCookiesFromResponse(response);
    completionHandler(request);
  } else {
    // No need to continue the redirect if there is no more bridge instance.
    completionHandler(NULL);
  }
}

#pragma mark - Private

- (void)requestCompletedWithData:(NSData*)data
                        response:(NSURLResponse*)response
                           error:(NSError*)error {
  if (!self.bridge)
    return;
  NSHTTPURLResponse* responseWithHeaders =
      base::apple::ObjCCastStrict<NSHTTPURLResponse>(response);
  if (error) {
    VLOG(1) << "Fetch failed: "
            << base::SysNSStringToUTF8(error.localizedDescription);
    self.bridge->OnURLFetchFailure(net::ERR_FAILED,
                                   responseWithHeaders.statusCode);
  } else {
    self.bridge->SetCanonicalCookiesFromResponse(responseWithHeaders);
    NSString* result = [[NSString alloc] initWithData:data
                                             encoding:NSUTF8StringEncoding];
    self.bridge->OnURLFetchSuccess(base::SysNSStringToUTF8(result),
                                   responseWithHeaders.statusCode);
  }
}

@end

GaiaAuthFetcherIOSNSURLSessionBridge::GaiaAuthFetcherIOSNSURLSessionBridge(
    GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate* delegate,
    web::BrowserState* browser_state)
    : GaiaAuthFetcherIOSBridge(delegate), browser_state_(browser_state) {
  url_session_delegate_ = [[GaiaAuthFetcherIOSURLSessionDelegate alloc] init];
  url_session_delegate_.bridge = this;
}

GaiaAuthFetcherIOSNSURLSessionBridge::~GaiaAuthFetcherIOSNSURLSessionBridge() {
  url_session_delegate_.bridge = nullptr;
}

void GaiaAuthFetcherIOSNSURLSessionBridge::Fetch(
    const GURL& url,
    const std::string& headers,
    const std::string& body,
    bool should_use_xml_http_request) {
  DCHECK(!request_.pending);

  request_ = Request(url, headers, body, should_use_xml_http_request);
  network::mojom::CookieManager* cookie_manager =
      browser_state_->GetCookieManager();
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  cookie_manager->GetCookieList(
      request_.url, options, net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(
          &GaiaAuthFetcherIOSNSURLSessionBridge::FetchPendingRequestWithCookies,
          base::Unretained(this)));
}

void GaiaAuthFetcherIOSNSURLSessionBridge::Cancel() {
  [url_session_data_task_ cancel];
  VLOG(1) << "Fetch was cancelled";
  OnURLFetchFailure(net::ERR_ABORTED, 0);
}

void GaiaAuthFetcherIOSNSURLSessionBridge::OnURLFetchSuccess(
    const std::string& data,
    int response_code) {
  if (!request_.pending) {
    return;
  }
  GURL url = FinishPendingRequest();
  delegate()->OnFetchComplete(url, data, net::OK, response_code);
}

void GaiaAuthFetcherIOSNSURLSessionBridge::OnURLFetchFailure(
    int error,
    int response_code) {
  if (!request_.pending) {
    return;
  }
  GURL url = FinishPendingRequest();
  delegate()->OnFetchComplete(url, std::string(),
                              static_cast<net::Error>(error), response_code);
}

NSURLRequest* GaiaAuthFetcherIOSNSURLSessionBridge::GetNSURLRequest() const {
  DCHECK(request_.pending);
  NSMutableURLRequest* request = [[NSMutableURLRequest alloc]
      initWithURL:net::NSURLWithGURL(request_.url)];
  net::HttpRequestHeaders request_headers;
  request_headers.AddHeadersFromString(request_.headers);
  for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();) {
    [request setValue:base::SysUTF8ToNSString(it.value())
        forHTTPHeaderField:base::SysUTF8ToNSString(it.name())];
  }
  if (!request_.body.empty()) {
    NSData* post_data = [base::SysUTF8ToNSString(request_.body)
        dataUsingEncoding:NSUTF8StringEncoding];
    [request setHTTPBody:post_data];
    [request setHTTPMethod:@"POST"];
    DCHECK(![[request allHTTPHeaderFields] objectForKey:@"Content-Type"]);
    [request setValue:@"application/x-www-form-urlencoded"
        forHTTPHeaderField:@"Content-Type"];
  }
  return request;
}

void GaiaAuthFetcherIOSNSURLSessionBridge::SetCanonicalCookiesFromResponse(
    NSHTTPURLResponse* response) {
  NSArray* cookies =
      [NSHTTPCookie cookiesWithResponseHeaderFields:response.allHeaderFields
                                             forURL:response.URL];
  network::mojom::CookieManager* cookie_manager =
      browser_state_->GetCookieManager();
  for (NSHTTPCookie* cookie : cookies) {
    std::unique_ptr<net::CanonicalCookie> canonical_cookie =
        net::CanonicalCookieFromSystemCookie(cookie, base::Time::Now());
    if (!canonical_cookie)
      continue;
    net::CookieOptions options;
    options.set_include_httponly();
    // Permit it to set a SameSite cookie if it wants to.
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    cookie_manager->SetCanonicalCookie(*std::move(canonical_cookie),
                                       net::GURLWithNSURL(response.URL),
                                       options, base::DoNothing());
  }
}

void GaiaAuthFetcherIOSNSURLSessionBridge::FetchPendingRequestWithCookies(
    const net::CookieAccessResultList& cookies_with_access_results,
    const net::CookieAccessResultList& excluded_cookies) {
  DCHECK(!url_session_);
  url_session_ = CreateNSURLSession(url_session_delegate_);
  url_session_delegate_.requestSession = url_session_;
  DCHECK(!url_session_data_task_);
  __weak __typeof(url_session_delegate_) weakDelegate = url_session_delegate_;
  url_session_data_task_ =
      [url_session_ dataTaskWithRequest:GetNSURLRequest()
                      completionHandler:^(NSData* data, NSURLResponse* response,
                                          NSError* error) {
                        [weakDelegate requestCompletedWithData:data
                                                      response:response
                                                         error:error];
                      }];
  NSMutableArray* http_cookies = [[NSMutableArray alloc]
      initWithCapacity:cookies_with_access_results.size()];
  for (const auto& cookie_with_access_result : cookies_with_access_results) {
    // `CHROME_CONNECTED` cookie is attached to all web requests to Google web
    // properties. Requests initiated from the browser services (e.g.
    // GaiaCookieManagerService) must not include this cookie.
    if (cookie_with_access_result.cookie.Name() ==
        signin::kChromeConnectedCookieName) {
      continue;
    }
    [http_cookies addObject:net::SystemCookieFromCanonicalCookie(
                                cookie_with_access_result.cookie)];
  }
  [url_session_.configuration.HTTPCookieStorage
      storeCookies:http_cookies
           forTask:url_session_data_task_];

  [url_session_data_task_ resume];
}

GURL GaiaAuthFetcherIOSNSURLSessionBridge::FinishPendingRequest() {
  DCHECK(request_.pending);
  GURL url = request_.url;
  request_ = Request();
  return url;
}

NSURLSession* GaiaAuthFetcherIOSNSURLSessionBridge::CreateNSURLSession(
    id<NSURLSessionTaskDelegate> url_session_delegate) {
  NSURLSessionConfiguration* session_configuration =
      NSURLSessionConfiguration.ephemeralSessionConfiguration;
  session_configuration.HTTPShouldSetCookies = YES;
  std::string user_agent =
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE);
  session_configuration.HTTPAdditionalHeaders = @{
    base::SysUTF8ToNSString(net::HttpRequestHeaders::kUserAgent) :
        base::SysUTF8ToNSString(user_agent),
  };
  return [NSURLSession sessionWithConfiguration:session_configuration
                                       delegate:url_session_delegate
                                  delegateQueue:NSOperationQueue.mainQueue];
}
