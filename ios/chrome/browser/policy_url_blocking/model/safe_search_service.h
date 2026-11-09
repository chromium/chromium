// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_SAFE_SEARCH_SERVICE_H_
#define IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_SAFE_SEARCH_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_search_api {
class URLChecker;
}  // namespace safe_search_api

// `SafeSearchService` provides access to `safe_search_api::URLChecker` for a
// given `ProfileIOS`.
class SafeSearchService : public KeyedService {
 public:
  using CheckSafeSearchCallback = base::OnceCallback<void(bool is_safe)>;

  explicit SafeSearchService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  SafeSearchService(const SafeSearchService&) = delete;
  SafeSearchService& operator=(const SafeSearchService&) = delete;

  ~SafeSearchService() override;

  // Starts a call to the SafeSearch API for the given URL to determine whether
  // the URL is safe. Returns whether `callback` was run synchronously.
  bool CheckSafeSearchURL(const GURL& url, CheckSafeSearchCallback callback);

  // Sets a custom SafeSearch `URLChecker` for testing.
  void SetSafeSearchURLCheckerForTest(
      std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker);

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker_;
};

#endif  // IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_SAFE_SEARCH_SERVICE_H_
