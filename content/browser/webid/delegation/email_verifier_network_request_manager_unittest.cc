// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_network_request_manager.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

class EmailVerifierNetworkRequestManagerTest : public ::testing::Test {
 public:
  EmailVerifierNetworkRequestManagerTest() = default;
  ~EmailVerifierNetworkRequestManagerTest() override = default;

 protected:
  void SetUp() override {
    url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
    manager_ = std::make_unique<EmailVerifierNetworkRequestManager>(
        rp_origin,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        network::mojom::ClientSecurityState::New(), content::FrameTreeNodeId());
  }

  content::BrowserTaskEnvironment task_environment_;
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
                             base::DoNothing());
  run_loop.Run();
  EXPECT_TRUE(called);
}

}  // namespace content::webid
