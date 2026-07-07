// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_GAIA_AUTH_FETCHER_IOS_BRIDGE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_GAIA_AUTH_FETCHER_IOS_BRIDGE_H_

#include <memory>
#include <string>

#import "base/memory/raw_ptr.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class GURL;
@class NSURLRequest;

// Interface for fetching Gaia auth requests that include the cookies on iOS.
class GaiaAuthFetcherIOSBridge {
 public:
  // Delegate class receives notification when the request is done.
  class Delegate {
   public:
    Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate();

    // Called when the request is done.
    virtual void OnFetchComplete(const GURL& url,
                                 const std::string& data,
                                 net::Error net_error,
                                 int response_code) = 0;
  };

  // Initializes the instance.
  GaiaAuthFetcherIOSBridge(Delegate* delegate);

  GaiaAuthFetcherIOSBridge(const GaiaAuthFetcherIOSBridge&) = delete;
  GaiaAuthFetcherIOSBridge& operator=(const GaiaAuthFetcherIOSBridge&) = delete;

  virtual ~GaiaAuthFetcherIOSBridge();

  // Starts a network fetch.
  // * `url` is the URL to fetch.
  // * `headers` are the HTTP headers to add to the request.
  // * `body` is the HTTP body to add to the request. If not empty, the fetch
  //   will be a POST request.
  //
  // Implementations are expected to call
  // Delegate::OnFetchComplete() when the fetch
  // operation is finished.
  virtual void Fetch(const GURL& url,
                     const std::string& headers,
                     const std::string& body,
                     bool should_use_xml_http_request) = 0;

  // Cancels the current fetch.
  //
  // Implementations are expected to call
  // Delegate::OnFetchComplete() with error
  // `net::ERR_ABORTED`.
  virtual void Cancel() = 0;

 protected:
  Delegate* delegate() { return delegate_; }

 private:
  raw_ptr<Delegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_GAIA_AUTH_FETCHER_IOS_BRIDGE_H_
