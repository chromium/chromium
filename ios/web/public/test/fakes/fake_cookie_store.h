// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_COOKIE_STORE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_COOKIE_STORE_H_

#include <optional>

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

  void SetCanonicalCookieAsync(
      std::unique_ptr<net::CanonicalCookie> cookie,
      const GURL& source_url,
      const net::CookieOptions& options,
      SetCookiesCallback callback,
      std::optional<net::CookieAccessResult> cookie_access_result =
          std::nullopt) override;
  void GetCookieListWithOptionsAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
      GetCookieListCallback callback) override;
  void DeleteCanonicalCookieAsync(const net::CanonicalCookie& cookie,
                                  DeleteCallback callback) override;
  void DeleteAllCreatedInTimeRangeAsync(
      const net::CookieDeletionInfo::TimeRange& creation_range,
      DeleteCallback callback) override;
  void DeleteAllMatchingInfoAsync(net::CookieDeletionInfo delete_info,
                                  DeleteCallback callback) override;
  void DeleteSessionCookiesAsync(DeleteCallback callback) override;
  void DeleteMatchingCookiesAsync(DeletePredicate predicate,
                                  DeleteCallback callback) override;
  void FlushStore(base::OnceClosure callback) override;
  void SetCookieableSchemes(const std::vector<std::string>& schemes,
                            SetCookieableSchemesCallback callback) override;
  net::CookieChangeDispatcher& GetChangeDispatcher() override;

 private:
  net::CookieList all_cookies_;
  net::CookieAccessResultList excluded_list_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_COOKIE_STORE_H_
