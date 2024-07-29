// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_FAKE_SAFE_BROWSING_SERVICE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_FAKE_SAFE_BROWSING_SERVICE_H_

#include <string>

#include "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_url_loader_factory.h"

// A fake SafeBrowsingService whose database treats URLs from host
// safe.browsing.unsafe.chromium.test as unsafe, and treats all other URLs as
// safe.
class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  // URLs with this host are treated as unsafe by all fake checkers.
  static const std::string kUnsafeHost;

  // URLs with this host are treated as unsafe only by async fake checkers.
  static const std::string kAsyncUnsafeHost;

  FakeSafeBrowsingService();

  FakeSafeBrowsingService(const FakeSafeBrowsingService&) = delete;
  FakeSafeBrowsingService& operator=(const FakeSafeBrowsingService&) = delete;

  // SafeBrowsingService:
  void Initialize(PrefService* prefs,
                  const base::FilePath& user_data_path,
                  safe_browsing::SafeBrowsingMetricsCollector*
                      safe_browsing_metrics_collector) override;
  void ShutDown() override;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> CreateUrlChecker(
      network::mojom::RequestDestination request_destination,
      web::WebState* web_state,
      SafeBrowsingClient* client) override;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> CreateSyncChecker(
      network::mojom::RequestDestination request_destination,
      web::WebState* web_state,
      SafeBrowsingClient* client) override;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> CreateAsyncChecker(
      network::mojom::RequestDestination request_destination,
      web::WebState* web_state,
      SafeBrowsingClient* client) override;
  bool ShouldCreateAsyncChecker(web::WebState* web_state,
                                SafeBrowsingClient* client) override;
  bool CanCheckUrl(const GURL& url) const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> GetDatabaseManager()
      override;
  network::mojom::NetworkContext* GetNetworkContext() override;
  void ClearCookies(const net::CookieDeletionInfo::TimeRange& creation_range,
                    base::OnceClosure callback) override;

 protected:
  ~FakeSafeBrowsingService() override;

 private:
  network::TestURLLoaderFactory url_loader_factory_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_FAKE_SAFE_BROWSING_SERVICE_H_
