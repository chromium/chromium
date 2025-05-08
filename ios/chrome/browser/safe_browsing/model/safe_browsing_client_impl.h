// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_IMPL_H_

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"

class PrerenderService;

namespace enterprise_connectors {
class ConnectorsService;
}

// ios/chrome implementation of SafeBrowsingClient.
class SafeBrowsingClientImpl : public SafeBrowsingClient {
 public:
  using UrlLookupServiceFactory =
      base::RepeatingCallback<safe_browsing::RealTimeUrlLookupServiceBase*()>;

  SafeBrowsingClientImpl(
      PrefService* pref_Service,
      safe_browsing::HashRealTimeService* hash_real_time_service,
      PrerenderService* prerender_service,
      UrlLookupServiceFactory url_lookup_service_factory,
      enterprise_connectors::ConnectorsService* connectors_service);

  ~SafeBrowsingClientImpl() override;

  // SafeBrowsingClient implementation.
  base::WeakPtr<SafeBrowsingClient> AsWeakPtr() override;
  PrefService* GetPrefs() override;
  SafeBrowsingService* GetSafeBrowsingService() override;
  safe_browsing::RealTimeUrlLookupServiceBase* GetRealTimeUrlLookupService()
      override;
  safe_browsing::HashRealTimeService* GetHashRealTimeService() override;
  variations::VariationsService* GetVariationsService() override;
  bool ShouldBlockUnsafeResource(
      const security_interstitials::UnsafeResource& resource) const override;
  bool OnMainFrameUrlQueryCancellationDecided(web::WebState* web_state,
                                              const GURL& url) override;
  bool ShouldForceSyncRealTimeUrlChecks() const override;

 private:
  raw_ptr<PrefService> pref_service_;
  raw_ptr<safe_browsing::HashRealTimeService> hash_real_time_service_;
  raw_ptr<PrerenderService> prerender_service_;
  // When enterprise Url filtering is enabled, this factory returns the
  // enterprise Url lookup service. Otherwise, it returns the consumer service.
  UrlLookupServiceFactory url_lookup_service_factory_;
  // Unowned pointer used for determining if real time url checks should be done
  // synchronously due to Enteprise Url Filtering enabled. Must not be null.
  raw_ptr<enterprise_connectors::ConnectorsService> connectors_service_;

  // Must be last.
  base::WeakPtrFactory<SafeBrowsingClientImpl> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_IMPL_H_
