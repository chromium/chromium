// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"

#import "base/memory/raw_ptr.h"

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_IMPL_H_

class PrerenderService;

// ios/chrome implementation of SafeBrowsingClient.
class SafeBrowsingClientImpl : public SafeBrowsingClient {
 public:
  SafeBrowsingClientImpl(
      safe_browsing::RealTimeUrlLookupService* lookup_service,
      safe_browsing::HashRealTimeService* hash_real_time_service,
      PrerenderService* prerender_service);

  ~SafeBrowsingClientImpl() override;

  // SafeBrowsingClient implementation.
  base::WeakPtr<SafeBrowsingClient> AsWeakPtr() override;
  SafeBrowsingService* GetSafeBrowsingService() override;
  safe_browsing::RealTimeUrlLookupService* GetRealTimeUrlLookupService()
      override;
  safe_browsing::HashRealTimeService* GetHashRealTimeService() override;
  variations::VariationsService* GetVariationsService() override;
  bool ShouldBlockUnsafeResource(
      const security_interstitials::UnsafeResource& resource) const override;
  bool OnMainFrameUrlQueryCancellationDecided(web::WebState* web_state,
                                              const GURL& url) override;

 private:
  raw_ptr<safe_browsing::RealTimeUrlLookupService> lookup_service_;
  raw_ptr<safe_browsing::HashRealTimeService> hash_real_time_service_;
  raw_ptr<PrerenderService> prerender_service_;

  // Must be last.
  base::WeakPtrFactory<SafeBrowsingClientImpl> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_IMPL_H_
