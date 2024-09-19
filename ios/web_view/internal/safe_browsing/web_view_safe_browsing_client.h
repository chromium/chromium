// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_SAFE_BROWSING_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_SAFE_BROWSING_CLIENT_H_

#include "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"

class WebViewSafeBrowsingClient : public SafeBrowsingClient {
 public:
  WebViewSafeBrowsingClient();

  ~WebViewSafeBrowsingClient() override;

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
  // Must be last.
  base::WeakPtrFactory<WebViewSafeBrowsingClient> weak_factory_{this};
};

#endif  // IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_SAFE_BROWSING_CLIENT_H_
