// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_COOKIES_WK_HTTP_SYSTEM_COOKIE_STORE_H_
#define IOS_WEB_NET_COOKIES_WK_HTTP_SYSTEM_COOKIE_STORE_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/net/cookies/system_cookie_store.h"

namespace web {

class WKWebViewConfigurationProvider;

// This class is an implementation of SystemCookieStore, WKHTTPSystemCookieStore
// uses WKHTTPCookieStore as the underlying system cookie store.
class WKHTTPSystemCookieStore : public net::SystemCookieStore {
 public:
  explicit WKHTTPSystemCookieStore(
      WKWebViewConfigurationProvider* config_provider);

  WKHTTPSystemCookieStore(const WKHTTPSystemCookieStore&) = delete;
  WKHTTPSystemCookieStore& operator=(const WKHTTPSystemCookieStore&) = delete;

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
  // Forward-declaration of implementation details.
  class Helper;

  // Filters `cookies` to match `include_url`, sorts based on RFC6265 using
  // `weak_time_manager`. This is not a free function because it depends on
  // being a friend with CookieCreationTimeManager.
  static NSArray<NSHTTPCookie*>* FilterAndSortCookies(
      base::WeakPtr<net::CookieCreationTimeManager> weak_time_manager,
      const GURL& include_url,
      NSArray<NSHTTPCookie*>* cookies);

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<Helper> helper_;
};

}  // namespace web

#endif  // IOS_WEB_NET_COOKIES_WK_HTTP_SYSTEM_COOKIE_STORE_H_
