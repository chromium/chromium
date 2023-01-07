// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_NS_HTTP_SYSTEM_COOKIE_STORE_H_
#define IOS_NET_COOKIES_NS_HTTP_SYSTEM_COOKIE_STORE_H_

#import <Foundation/Foundation.h>

#import "ios/net/cookies/system_cookie_store.h"

namespace net {

// This class is an implementation of SystemCookieStore, NSHTTPSystemCookieStore
// uses NSHTTPCookieStorage as the underlying system cookie store.
class NSHTTPSystemCookieStore : public net::SystemCookieStore {
 public:
  // By default the underlying cookiestore is
  // |NSHTTPCookieStorage sharedHTTPCookieStorage|
  NSHTTPSystemCookieStore();

  explicit NSHTTPSystemCookieStore(NSHTTPCookieStorage* cookie_store);

  NSHTTPSystemCookieStore(const NSHTTPSystemCookieStore&) = delete;
  NSHTTPSystemCookieStore& operator=(const NSHTTPSystemCookieStore&) = delete;

  ~NSHTTPSystemCookieStore() override;

  // Gets cookies for URL and calls |callback| async on these cookies.
  void GetCookiesForURLAsync(const GURL& url,
                             SystemCookieCallbackForCookies callback) override;

  // Gets all cookies and calls |callback| async on these cookies.
  void GetAllCookiesAsync(SystemCookieCallbackForCookies callback) override;

  // Deletes specific cookie and calls |callback| async after that.
  void DeleteCookieAsync(NSHTTPCookie* cookie,
                         SystemCookieCallback callback) override;

  // Sets cookie, and calls |callback| async after that.
  void SetCookieAsync(NSHTTPCookie* cookie,
                      const base::Time* optional_creation_time,
                      SystemCookieCallback callback) override;

  // Clears all cookies from the store and call |callback| after all cookies are
  // deleted.
  void ClearStoreAsync(SystemCookieCallback callback) override;

  NSHTTPCookieAcceptPolicy GetCookieAcceptPolicy() override;

 private:
  // Returns all cookies for a specific |url| from the internal cookie store.
  // Cookies are sorted, as per RFC6265.
  NSArray* GetCookiesForURL(const GURL& url);

  // Returns all cookies from the internal http cookie store.
  // Cookies are sorted, as per RFC6265.
  NSArray* GetAllCookies();

  // Deletes a specific cookie from the internal http cookie store.
  void DeleteCookie(NSHTTPCookie* cookie);

  // Sets a specific cookie to the internal http cookie store.
  // if the |optional_creation_time| is nullptr, uses Time::Now() as the
  // creation time.
  void SetCookie(NSHTTPCookie* cookie,
                 const base::Time* optional_creation_time);

  // Clears all cookies from the internal cookie store.
  void ClearStore();

  NSHTTPCookieStorage* cookie_store_;
};

}  // namespace net

#endif  // IOS_NET_COOKIES_NS_HTTP_SYSTEM_COOKIE_STORE_H_
