// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_GAIA_AUTH_FETCHER_IOS_NS_URL_SESSION_BRIDGE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_GAIA_AUTH_FETCHER_IOS_NS_URL_SESSION_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <string>
#include <vector>

#import "base/memory/raw_ptr.h"
#include "ios/chrome/browser/signin/model/gaia_auth_fetcher_ios_bridge.h"
#include "net/cookies/canonical_cookie.h"

class GaiaAuthFetcherIOSNSURLSessionBridge;
@class GaiaAuthFetcherIOSURLSessionDelegate;
@class NSHTTPURLResponse;
@class NSURLSession;

namespace web {
class BrowserState;
}

// Specialization of GaiaAuthFetcher on iOS, using NSURLSession to send
// requests.
class GaiaAuthFetcherIOSNSURLSessionBridge : public GaiaAuthFetcherIOSBridge {
 public:
  GaiaAuthFetcherIOSNSURLSessionBridge(
      GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate* delegate,
      web::BrowserState* browser_state);

  GaiaAuthFetcherIOSNSURLSessionBridge(
      const GaiaAuthFetcherIOSNSURLSessionBridge&) = delete;
  GaiaAuthFetcherIOSNSURLSessionBridge& operator=(
      const GaiaAuthFetcherIOSNSURLSessionBridge&) = delete;

  ~GaiaAuthFetcherIOSNSURLSessionBridge() override;

  // GaiaAuthFetcherIOSBridge:
  void Fetch(const GURL& url,
             const std::string& headers,
             const std::string& body,
             bool should_use_xml_http_request) override;
  void Cancel() override;

  // Informs the bridge of the success of the URL fetch.
  // * `data` is the body of the HTTP response.
  // * `response_code` is the response code.
  // URLFetchSuccess and URLFetchFailure are no-op if one of them was already
  // called.
  void OnURLFetchSuccess(const std::string& data, int response_code);

  // Informs the bridge of the failure of the URL fetch.
  // * `is_cancelled` whether the fetch failed because it was cancelled.
  // URLFetchSuccess and URLFetchFailure are no-op if one of them was already
  // called.
  void OnURLFetchFailure(int error, int response_code);

  // Set cookies from `response` in SystemCookieStore asynchronously.
  void SetCanonicalCookiesFromResponse(NSHTTPURLResponse* response);

 private:
  // A network request that needs to be fetched.
  struct Request {
    Request();
    Request(const GURL& url,
            const std::string& headers,
            const std::string& body,
            bool should_use_xml_http_request);
    // Whether the request is pending (i.e. awaiting to be processed or
    // currently being processed).
    bool pending;
    // URL to fetch.
    GURL url;
    // HTTP headers to add to the request.
    std::string headers;
    // HTTP body to add to the request.
    std::string body;
    // Whether XmlHTTPRequest should be injected in JS instead of using
    // WKWebView directly.
    bool should_use_xml_http_request;
  };

  friend class GaiaAuthFetcherIOSNSURLSessionBridgeTest;

  // Creates a NSURLRequest with the url, the headers and the body received in
  // the constructor of this instance. The request is a GET if `body` is empty
  // and a POST otherwise.
  NSURLRequest* GetNSURLRequest() const;

  // Fetches the pending request if it exists. Updates the cookie store for
  // each redirect and call either URLFetchSuccess() or URLFetchFailure().
  void FetchPendingRequest();

  // Finishes the pending request and cleans up its associated state. Returns
  // the URL of the request.
  GURL FinishPendingRequest();

  // Starts the NSURLRequest with the cookie list.
  void FetchPendingRequestWithCookies(
      const net::CookieAccessResultList& cookies_with_access_results,
      const net::CookieAccessResultList& excluded_cookies);
  // Creates a NSURLSession, and sets its delegate.
  virtual NSURLSession* CreateNSURLSession(
      id<NSURLSessionTaskDelegate> url_session_delegate);

  // Browser state associated with the bridge.
  raw_ptr<web::BrowserState> browser_state_;

  // Request currently processed by the bridge.
  Request request_;

  // Fetcher which makes requests to Gaia with NSURLSession.
  GaiaAuthFetcherIOSURLSessionDelegate* url_session_delegate_;

  // Session to send the NSURLRequest.
  NSURLSession* url_session_;

  // Task to send the NSURLRequest.
  NSURLSessionDataTask* url_session_data_task_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_GAIA_AUTH_FETCHER_IOS_NS_URL_SESSION_BRIDGE_H_
