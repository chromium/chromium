// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_NS_URL_SESSION_BRIDGE_H_
#define IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_NS_URL_SESSION_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <vector>

#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios_bridge.h"
#include "net/cookies/canonical_cookie.h"

class GaiaAuthFetcherIOSNSURLSessionBridge;
@class GaiaAuthFetcherIOSURLSessionDelegate;
@class NSHTTPURLResponse;
@class NSURLSession;

// Specialization of GaiaAuthFetcher on iOS, using NSURLSession to send
// requests. This class can only be used when those 2 flags are enabbled:
//  + kUseNSURLSessionForGaiaSigninRequests
//  + web::features::kWKHTTPSystemCookieStore
class GaiaAuthFetcherIOSNSURLSessionBridge : public GaiaAuthFetcherIOSBridge {
 public:
  GaiaAuthFetcherIOSNSURLSessionBridge(
      GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate* delegate,
      web::BrowserState* browser_state);
  ~GaiaAuthFetcherIOSNSURLSessionBridge() override;

  // GaiaAuthFetcherIOSBridge.
  void Cancel() override;

  // Set cookies from |response| in SystemCookieStore asynchronously.
  void SetCanonicalCookiesFromResponse(NSHTTPURLResponse* response);

 private:
  friend class GaiaAuthFetcherIOSNSURLSessionBridgeTest;

  // GaiaAuthFetcherIOSBridge.
  void FetchPendingRequest() override;

  // Starts the NSURLRequest with the cookie list.
  void FetchPendingRequestWithCookies(
      const net::CookieStatusList& cookies,
      const net::CookieStatusList& excluded_cookies);
  // Creates a NSURLSession, and sets its delegate.
  virtual NSURLSession* CreateNSURLSession(
      id<NSURLSessionTaskDelegate> url_session_delegate);

  // Fetcher which makes requests to Gaia with NSURLSession.
  GaiaAuthFetcherIOSURLSessionDelegate* url_session_delegate_;
  // Session to send the NSURLRequest.
  NSURLSession* url_session_;
  // Task to send the NSURLRequest.
  NSURLSessionDataTask* url_session_data_task_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcherIOSNSURLSessionBridge);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_NS_URL_SESSION_BRIDGE_H_
