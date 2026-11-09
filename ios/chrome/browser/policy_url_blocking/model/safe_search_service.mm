// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/model/safe_search_service.h"

#import <memory>
#import <utility>

#import "base/check_is_test.h"
#import "base/functional/bind.h"
#import "components/safe_search_api/safe_search/safe_search_url_checker_client.h"
#import "components/safe_search_api/url_checker.h"
#import "components/url_matcher/url_util.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "url/gurl.h"

namespace {

// Calls the `SafeSearchService` callback with the result of the SafeSearch API
// check.
void OnCheckURLDone(SafeSearchService::CheckSafeSearchCallback callback,
                    const GURL& /* url */,
                    safe_search_api::Classification classification,
                    safe_search_api::ClassificationDetails details) {
  std::move(callback).Run(classification ==
                          safe_search_api::Classification::SAFE);
}

}  // namespace

SafeSearchService::SafeSearchService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

SafeSearchService::~SafeSearchService() = default;

bool SafeSearchService::CheckSafeSearchURL(const GURL& url,
                                           CheckSafeSearchCallback callback) {
  if (!safe_search_url_checker_) {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("safe_search_service_ios", R"(
          semantics {
            sender: "Cloud Policy"
            description:
              "Checks whether a given URL is considered safe by Google "
              "SafeSearch on iOS."
            trigger:
              "If the SafeSitesFilterBehavior policy is enabled, sent on "
              "navigations."
            data: "URL to be checked."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature is controlled by enterprise policy "
              "SafeSitesFilterBehavior."
            chrome_policy {
              SafeSitesFilterBehavior {
                SafeSitesFilterBehavior: 0
              }
            }
          })");

    safe_search_url_checker_ = std::make_unique<safe_search_api::URLChecker>(
        std::make_unique<safe_search_api::SafeSearchURLCheckerClient>(
            url_loader_factory_, traffic_annotation));
  }

  return safe_search_url_checker_->CheckURL(
      url_matcher::util::Normalize(url),
      base::BindOnce(&OnCheckURLDone, std::move(callback)));
}

void SafeSearchService::SetSafeSearchURLCheckerForTest(
    std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker) {
  CHECK_IS_TEST();
  safe_search_url_checker_ = std::move(safe_search_url_checker);
}
