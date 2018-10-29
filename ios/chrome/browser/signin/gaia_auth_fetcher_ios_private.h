// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_PRIVATE_H_
#define IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_PRIVATE_H_

#import <WebKit/WebKit.h>

#include "base/macros.h"
#include "components/signin/ios/browser/active_state_manager.h"

class GaiaAuthFetcherIOS;
class GURL;

namespace web {
class BrowserState;
}

// Navigation delegate attached to a WKWebView used for URL fetches.
@interface GaiaAuthFetcherNavigationDelegate : NSObject<WKNavigationDelegate>
@end

// Bridge between the GaiaAuthFetcherIOS and the webview (and its navigation
// delegate) used to actually do the network fetch.
class GaiaAuthFetcherIOSBridge : ActiveStateManager::Observer {
 public:
  GaiaAuthFetcherIOSBridge(GaiaAuthFetcherIOS* fetcher,
                           web::BrowserState* browser_state);
  virtual ~GaiaAuthFetcherIOSBridge();

  // Starts a network fetch.
  // * |url| is the URL to fetch.
  // * |headers| are the HTTP headers to add to the request.
  // * |body| is the HTTP body to add to the request. If not empty, the fetch
  //   will be a POST request.
  void Fetch(const GURL& url,
             const std::string& headers,
             const std::string& body,
             bool shouldUseXmlHTTPRequest);

  // Cancels the current fetch.
  void Cancel();

  // Informs the bridge of the success of the URL fetch.
  // * |data| is the body of the HTTP response.
  // URLFetchSuccess and URLFetchFailure are no-op if one of them was already
  // called.
  void URLFetchSuccess(const std::string& data);

  // Informs the bridge of the failure of the URL fetch.
  // * |is_cancelled| whether the fetch failed because it was cancelled.
  // URLFetchSuccess and URLFetchFailure are no-op if one of them was already
  // called.
  void URLFetchFailure(bool is_cancelled);

 private:
  friend class GaiaAuthFetcherIOSTest;

  // A network request that needs to be fetched.
  struct Request {
    Request();
    Request(const GURL& url,
            const std::string& headers,
            const std::string& body,
            bool shouldUseXmlHTTPRequest);
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
    bool shouldUseXmlHTTPRequest;
  };

  // Fetches the pending request if it exists.
  void FetchPendingRequest();
  // Finishes the pending request and cleans up its associated state. Returns
  // the URL of the request.
  GURL FinishPendingRequest();

  // Returns the cached WKWebView if it exists, or creates one if necessary.
  // Can return nil if the browser state is not active.
  WKWebView* GetWKWebView();
  // Actually creates a WKWebView. Virtual for testing.
  virtual WKWebView* BuildWKWebView();
  // Stops any page loading in the WKWebView currently in use and releases it.
  void ResetWKWebView();

  // ActiveStateManager::Observer implementation.
  void OnActive() override;
  void OnInactive() override;

  // Browser state associated with the bridge, used to create WKWebViews.
  web::BrowserState* browser_state_;
  // Fetcher owning this bridge.
  GaiaAuthFetcherIOS* fetcher_;
  // Request currently processed by the bridge.
  Request request_;
  // Navigation delegate of |web_view_| that informs the bridge of relevant
  // navigation events.
  GaiaAuthFetcherNavigationDelegate* navigation_delegate_;
  // Web view used to do the network requests.
  WKWebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcherIOSBridge);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_PRIVATE_H_
