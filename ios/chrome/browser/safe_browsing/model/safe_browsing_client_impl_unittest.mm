// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_impl.h"

#import "base/memory/ptr_util.h"
#import "base/test/bind.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/chrome/browser/prerender/model/fake_prerender_service.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"

class SafeBrowsingClientImplTest : public PlatformTest {
 protected:
  SafeBrowsingClientImplTest()
      : prerender_service_(base::WrapUnique(new FakePrerenderService())),
        client_(base::WrapUnique(
            new SafeBrowsingClientImpl(/*lookup_service=*/nullptr,
                                       /*hash_real_time_service=*/nullptr,
                                       prerender_service_.get()))),
        web_state_(base::WrapUnique(new web::FakeWebState())) {}

  // Configures `prerender_service_` to prerender `web_state_`.
  void PrerenderWebState() const {
    FakePrerenderService* fake_prerender_service =
        static_cast<FakePrerenderService*>(prerender_service_.get());
    fake_prerender_service->set_prerender_web_state(web_state_.get());
  }

  std::unique_ptr<PrerenderService> prerender_service_;
  std::unique_ptr<SafeBrowsingClientImpl> client_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Non prerendered webstates should not block unsafe resources.
TEST_F(SafeBrowsingClientImplTest,
       ShouldNotBlockUnsafeResourceIfNotPrerendered) {
  security_interstitials::UnsafeResource unsafe_resource;
  unsafe_resource.weak_web_state = web_state_->GetWeakPtr();
  EXPECT_FALSE(client_->ShouldBlockUnsafeResource(unsafe_resource));
}

// Prerendered webstates should block unsafe resources.
TEST_F(SafeBrowsingClientImplTest, ShouldBlockUnsafeResourceIfPrerendered) {
  PrerenderWebState();
  security_interstitials::UnsafeResource unsafe_resource;
  unsafe_resource.weak_web_state = web_state_->GetWeakPtr();
  EXPECT_TRUE(client_->ShouldBlockUnsafeResource(unsafe_resource));
}

// Verifies prerendering is cancelled when the main frame load is cancelled.
TEST_F(SafeBrowsingClientImplTest, ShouldCancelPrerenderInMainFrame) {
  GURL url = GURL("https://www.chromium.org");
  PrerenderWebState();
  prerender_service_->StartPrerender(url, web::Referrer(),
                                     ui::PAGE_TRANSITION_LINK, web_state_.get(),
                                     /*immediately=*/true);
  EXPECT_TRUE(prerender_service_->IsWebStatePrerendered(web_state_.get()));
  EXPECT_TRUE(prerender_service_->HasPrerenderForUrl(url));
  client_->OnMainFrameUrlQueryCancellationDecided(web_state_.get(), url);
  EXPECT_FALSE(prerender_service_->HasPrerenderForUrl(url));
}
