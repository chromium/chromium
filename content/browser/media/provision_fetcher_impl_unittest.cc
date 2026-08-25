// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/provision_fetcher_impl.h"

#include "base/run_loop.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "media/mojo/mojom/provision_fetcher.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kFirstUrl[] = "https://a.com/";
constexpr char kSecondUrl[] = "https://b.com/";

RenderFrameHost* SimulateNavigation(RenderFrameHost* rfh, const GURL& url) {
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(url, rfh);
  navigation_simulator->Commit();
  return navigation_simulator->GetFinalRenderFrameHost();
}

}  // namespace

class ProvisionFetcherImplTest : public RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();
    SimulateNavigation(main_rfh(), GURL(kFirstUrl));
  }

  void TearDown() override {
    shared_url_loader_factory_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(ProvisionFetcherImplTest, DestroyedOnCrossDocumentNavigation) {
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);

  mojo::Remote<media::mojom::ProvisionFetcher> remote;
  ProvisionFetcherImpl::Create(main_rfh(), shared_url_loader_factory_,
                               remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  ASSERT_TRUE(remote.is_connected());

  SimulateNavigation(main_rfh(), GURL(kSecondUrl));
  base::RunLoop().RunUntilIdle();
  remote.FlushForTesting();

  EXPECT_FALSE(remote.is_connected());
}

TEST_F(ProvisionFetcherImplTest, DestroyedOnWebContentsTeardown) {
  mojo::Remote<media::mojom::ProvisionFetcher> remote;
  ProvisionFetcherImpl::Create(main_rfh(), shared_url_loader_factory_,
                               remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  ASSERT_TRUE(remote.is_connected());

  DeleteContents();
  base::RunLoop().RunUntilIdle();
  remote.FlushForTesting();

  EXPECT_FALSE(remote.is_connected());
}

}  // namespace content
