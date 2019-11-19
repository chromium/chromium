// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_COOKIES_WK_HTTP_SYSTEM_COOKIE_STORE_H_
#define IOS_WEB_NET_COOKIES_WK_HTTP_SYSTEM_COOKIE_STORE_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "ios/net/cookies/system_cookie_store.h"
#import "ios/web/net/cookies/crw_wk_http_cookie_store.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider_observer.h"

namespace web {

// This class is an implementation of SystemCookieStore, WKHTTPSystemCookieStore
// uses WKHTTPCookieStore as the underlying system cookie store.
class WKHTTPSystemCookieStore : public net::SystemCookieStore,
                                public WKWebViewConfigurationProviderObserver {
 public:
  explicit WKHTTPSystemCookieStore(
      WKWebViewConfigurationProvider* config_provider);

  ~WKHTTPSystemCookieStore() override;

  void GetCookiesForURLAsync(
      const GURL& url,
      net::SystemCookieStore::SystemCookieCallbackForCookies callback) override;

  void GetAllCookiesAsync(
      net::SystemCookieStore::SystemCookieCallbackForCookies callback) override;

  void DeleteCookieAsync(
      NSHTTPCookie* cookie,
      net::SystemCookieStore::SystemCookieCallback callback) override;

  void SetCookieAsync(
      NSHTTPCookie* cookie,
      const base::Time* optional_creation_time,
      net::SystemCookieStore::SystemCookieCallback callback) override;

  void ClearStoreAsync(
      net::SystemCookieStore::SystemCookieCallback callback) override;

  NSHTTPCookieAcceptPolicy GetCookieAcceptPolicy() override;

 private:
  // Gets cookies on UI Thread then processes the result and runs callback on IO
  // thread. If |include_url| is not empty, only include cookies that match it.
  void GetCookiesAsyncInternal(
      const GURL& include_url,
      net::SystemCookieStore::SystemCookieCallbackForCookies callback);

  // WKWebViewConfigurationProviderObserver:
  // Updates the internal WKHTTPCookieStore and its observer.
  void DidCreateNewConfiguration(
      WKWebViewConfigurationProvider* config_provider,
      WKWebViewConfiguration* new_config) override;

  // Filters |cookies| to match |include_url|, sorts based on RFC6265 using
  // |weak_time_manager| and then runs |callback|.
  // If |include_url| is empty then cookies are processed without filtering.
  static void ProcessGetCookiesResultInIOThread(
      net::SystemCookieStore::SystemCookieCallbackForCookies callback,
      base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager,
      const GURL& include_url,
      NSArray<NSHTTPCookie*>* cookies);

  // Using CRWWKHTTPCookieStore instead of using WKHTTPCookieStore directly to
  // work around several bugs on WKHTTPCookieStore.
  CRWWKHTTPCookieStore* crw_cookie_store_ = nil;

  DISALLOW_COPY_AND_ASSIGN(WKHTTPSystemCookieStore);
};

}  // namespace web

#endif  // IOS_WEB_NET_COOKIES_WK_HTTP_SYSTEM_COOKIE_STORE_H_
