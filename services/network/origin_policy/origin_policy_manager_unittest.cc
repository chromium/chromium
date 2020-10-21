// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_manager.h"
#include "base/optional.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/origin_policy.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

// Unit tests for OriginPolicyManager.
//
// Since OriginPolicyManager is the main integration point, it is mostly tested
// via integration tests using web platform tests. This tests only the aspects
// that are not web-exposed.

namespace network {

class OriginPolicyManagerTest : public testing::Test {
 public:
  OriginPolicyManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    network_service_ = NetworkService::CreateForTesting();

    auto context_params = mojom::NetworkContextParams::New();
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    // Use a fixed proxy config, to avoid dependencies on local network
    // configuration.
    context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));
    manager_ = std::make_unique<OriginPolicyManager>(network_context_.get());
  }

  void RetrieveOriginPolicyAndStoreResult(
      const url::Origin& origin,
      const base::Optional<std::string>& header) {
    manager_->RetrieveOriginPolicy(
        origin,
        net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                   origin, origin, net::SiteForCookies()),
        header,
        base::BindOnce(&OriginPolicyManagerTest::Callback,
                       base::Unretained(this)));
  }

  OriginPolicyManager* manager() { return manager_.get(); }
  OriginPolicy* result() const { return result_.get(); }

  NetworkContext* network_context() { return network_context_.get(); }

 private:
  void Callback(const OriginPolicy& result) {
    result_ = std::make_unique<OriginPolicy>(result);
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<OriginPolicyManager> manager_;
  std::unique_ptr<OriginPolicy> result_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyManagerTest);
};

TEST_F(OriginPolicyManagerTest, AddReceiver) {
  mojo::Remote<mojom::OriginPolicyManager> origin_policy_remote;
  EXPECT_EQ(0u, manager()->GetReceiversForTesting().size());

  manager()->AddReceiver(origin_policy_remote.BindNewPipeAndPassReceiver());

  EXPECT_EQ(1u, manager()->GetReceiversForTesting().size());
}

TEST_F(OriginPolicyManagerTest, AddExceptionFor) {
  auto origin = url::Origin::Create(GURL("https://example.com"));
  manager()->AddExceptionFor(origin);

  RetrieveOriginPolicyAndStoreResult(origin, "allowed=(\"my-policy\")");

  EXPECT_EQ(result()->state, OriginPolicyState::kNoPolicyApplies);
}

}  // namespace network
