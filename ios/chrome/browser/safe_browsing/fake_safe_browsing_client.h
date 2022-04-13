// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/scoped_refptr.h"
#include "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_FAKE_SAFE_BROWSING_CLIENT_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_FAKE_SAFE_BROWSING_CLIENT_H_

class SafeBrowsingService;

// Fake implementation of SafeBrowsingClient.
class FakeSafeBrowsingClient : public SafeBrowsingClient {
 public:
  FakeSafeBrowsingClient();
  ~FakeSafeBrowsingClient() override;

  // Controls the return value of |ShouldBlockUnsafeResource|.
  void set_should_block_unsafe_resource(bool should_block_unsafe_resource) {
    should_block_unsafe_resource_ = should_block_unsafe_resource;
  }

  // Controls the return value of |GetRealTimeUrlLookupService|.
  void set_real_time_url_lookup_service(
      safe_browsing::RealTimeUrlLookupService* lookup_service) {
    lookup_service_ = lookup_service;
  }

 private:
  // SafeBrowsingClient implementation.
  SafeBrowsingService* GetSafeBrowsingService() override;
  safe_browsing::RealTimeUrlLookupService* GetRealTimeUrlLookupService()
      override;
  bool ShouldBlockUnsafeResource(
      const security_interstitials::UnsafeResource& resource) const override;
  void OnMainFrameUrlQueryCancellationDecided(web::WebState* web_state,
                                              const GURL& url) const override;
  bool OnSubFrameUrlQueryCancellationDecided(web::WebState* web_state,
                                             const GURL& url) const override;

  scoped_refptr<SafeBrowsingService> safe_browsing_service_;
  bool should_block_unsafe_resource_ = false;
  safe_browsing::RealTimeUrlLookupService* lookup_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_FAKE_SAFE_BROWSING_CLIENT_H_
