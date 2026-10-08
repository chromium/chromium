// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/resource_broker/resource_broker_service_impl.h"

#include <utility>

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "content/services/resource_broker/public/mojom/resource_broker.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_broker {

namespace {

class ResourceBrokerServiceImplTest : public testing::Test {
 public:
  ResourceBrokerServiceImplTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ResourceBrokerServiceImplTest, ValidInitialization) {
  mojo::Remote<resource_broker::mojom::ResourceBrokerService> remote;
  ResourceBrokerServiceImpl service_impl(remote.BindNewPipeAndPassReceiver());

  base::UnguessableToken token = base::UnguessableToken::Create();
  auto valid_config = resource_broker::mojom::BrokerConfig::New();
  valid_config->session_nonce = token;
  remote->Initialize(std::move(valid_config));

  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
  ASSERT_TRUE(service_impl.config_for_testing());
  EXPECT_EQ(service_impl.config_for_testing()->session_nonce, token);
}

TEST_F(ResourceBrokerServiceImplTest, DoubleInitializeCrashes) {
  mojo::Remote<resource_broker::mojom::ResourceBrokerService> remote;
  ResourceBrokerServiceImpl service_impl(remote.BindNewPipeAndPassReceiver());

  base::UnguessableToken token1 = base::UnguessableToken::Create();
  auto config1 = resource_broker::mojom::BrokerConfig::New();
  config1->session_nonce = token1;
  service_impl.Initialize(std::move(config1));

  // Second Initialize call violates the invariant and must crash via CHECK.
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  auto config2 = resource_broker::mojom::BrokerConfig::New();
  config2->session_nonce = token2;
  EXPECT_CHECK_DEATH(service_impl.Initialize(std::move(config2)));
}

TEST_F(ResourceBrokerServiceImplTest, InvalidConfigCrashes) {
  mojo::Remote<resource_broker::mojom::ResourceBrokerService> remote;
  ResourceBrokerServiceImpl service_impl(remote.BindNewPipeAndPassReceiver());

  // Direct calls with invalid configs (null and empty nonce) violate the
  // invariant on trusted boundaries and must crash via CHECK.
  EXPECT_CHECK_DEATH(service_impl.Initialize(nullptr));
  EXPECT_CHECK_DEATH(
      service_impl.Initialize(resource_broker::mojom::BrokerConfig::New()));
}

}  // namespace

}  // namespace resource_broker
