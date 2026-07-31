// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

class EmailVerifierNetworkRequestManagerTest
    : public RenderViewHostTestHarness {
 public:
  EmailVerifierNetworkRequestManagerTest() = default;
  ~EmailVerifierNetworkRequestManagerTest() override = default;

 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    CHECK(main_rfh());
    url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
    manager_ = std::make_unique<EmailVerifierNetworkRequestManager>(
        rp_origin,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        network::mojom::ClientSecurityState::New(),
        main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetWeakDocumentPtr());
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<EmailVerifierNetworkRequestManager> manager_;
};

TEST_F(EmailVerifierNetworkRequestManagerTest,
       FetchWellKnownRequestDestination) {
  base::RunLoop run_loop;
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(network::mojom::RequestDestination::kEmailVerification,
                  request.destination);
        run_loop.Quit();
      });
  test_url_loader_factory_.SetInterceptor(interceptor);
  manager_->FetchWellKnown(GURL("https://idp.example"), base::DoNothing());
  run_loop.Run();
  EXPECT_TRUE(called);
}

TEST_F(EmailVerifierNetworkRequestManagerTest, SendTokenRequestDestination) {
  base::RunLoop run_loop;
  bool called = false;
  auto interceptor =
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        called = true;
        EXPECT_EQ(network::mojom::RequestDestination::kEmailVerification,
                  request.destination);
        run_loop.Quit();
      });
  test_url_loader_factory_.SetInterceptor(interceptor);
  manager_->SendTokenRequest(GURL("https://idp.example/token"), "data",
                             net::HttpRequestHeaders(), base::DoNothing());
  run_loop.Run();
  EXPECT_TRUE(called);
}

// If the frame is invalidated during the request initiation, the request is
// aborted.
TEST_F(EmailVerifierNetworkRequestManagerTest, RequestAbortedOnInvalidFrame) {
  url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
  network::TestURLLoaderFactory test_url_loader_factory;

  // Simulate the frame being invalidated by passing a `WeakDocumentPtr()` to
  // `EmailVerifierNetworkRequestManager`'s constructor.
  auto manager = std::make_unique<EmailVerifierNetworkRequestManager>(
      rp_origin,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory),
      network::mojom::ClientSecurityState::New(), FrameTreeNodeId(),
      WeakDocumentPtr());

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&](FetchStatus fetch_status,
          EmailVerifierNetworkRequestManager::WellKnown well_known) {
        // The request should be aborted.
        EXPECT_EQ(ParseStatus::kNoResponseError, fetch_status.parse_status);
        EXPECT_EQ(net::ERR_ABORTED, fetch_status.response_code);
        run_loop.Quit();
      });

  manager->FetchWellKnown(GURL("https://idp.example"), std::move(callback));
  run_loop.Run();

  EXPECT_EQ(0, test_url_loader_factory.NumPending());
}

}  // namespace content::webid
