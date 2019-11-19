// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_COOKIE_STORE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_COOKIE_STORE_H_

#include "net/cookies/cookie_store.h"

namespace web {

// Fake implementation for net::CookieStore interface. Can be used for testing.
class FakeCookieStore : public net::CookieStore {
 public:
  FakeCookieStore();
  ~FakeCookieStore() override;

  /// Sets cookies returned by GetAllCookiesAsync().
  void SetAllCookies(const net::CookieList& all_cookies);

  void GetAllCookiesAsync(GetAllCookiesCallback callback) override;

  // Methods below have not been implemented in this fake. Implement them when
  // necessary.

  void SetCanonicalCookieAsync(std::unique_ptr<net::CanonicalCookie> cookie,
                               std::string source_scheme,
                               const net::CookieOptions& options,
                               SetCookiesCallback callback) override;
  void GetCookieListWithOptionsAsync(const GURL& url,
                                     const net::CookieOptions& options,
                                     GetCookieListCallback callback) override;
  void DeleteCanonicalCookieAsync(const net::CanonicalCookie& cookie,
                                  DeleteCallback callback) override;
  void DeleteAllCreatedInTimeRangeAsync(
      const net::CookieDeletionInfo::TimeRange& creation_range,
      DeleteCallback callback) override;
  void DeleteAllMatchingInfoAsync(net::CookieDeletionInfo delete_info,
                                  DeleteCallback callback) override;
  void DeleteSessionCookiesAsync(DeleteCallback) override;
  void FlushStore(base::OnceClosure callback) override;
  void SetCookieableSchemes(const std::vector<std::string>& schemes,
                            SetCookieableSchemesCallback callback) override;
  net::CookieChangeDispatcher& GetChangeDispatcher() override;

 private:
  net::CookieList all_cookies_;
  net::CookieStatusList excluded_list_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_COOKIE_STORE_H_
