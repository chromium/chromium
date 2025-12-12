// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/devtools_durable_msg_collector_manager.h"

#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

using testing::ElementsAre;
using testing::UnorderedElementsAre;

class DevtoolsDurableMessageCollectorManagerTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DevtoolsDurableMessageCollectorManagerTest, AddCollector) {
  DevtoolsDurableMessageCollectorManager manager;
  mojo::Remote<mojom::DurableMessageCollector> collector_remote;
  manager.AddCollector(collector_remote.BindNewPipeAndPassReceiver());
  EXPECT_EQ(manager.GetCollectorsForTesting().size(), 1u);
}

TEST_F(DevtoolsDurableMessageCollectorManagerTest, CollectorDisconnects) {
  DevtoolsDurableMessageCollectorManager manager;
  mojo::Remote<mojom::DurableMessageCollector> collector_remote;
  manager.AddCollector(collector_remote.BindNewPipeAndPassReceiver());
  EXPECT_EQ(manager.GetCollectorsForTesting().size(), 1u);

  collector_remote.reset();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager.GetCollectorsForTesting().empty(); }));
}

TEST_F(DevtoolsDurableMessageCollectorManagerTest, EnableDisableForProfile) {
  DevtoolsDurableMessageCollectorManager manager;
  mojo::Remote<mojom::DurableMessageCollector> collector_remote;
  manager.AddCollector(collector_remote.BindNewPipeAndPassReceiver());
  ASSERT_EQ(manager.GetCollectorsForTesting().size(), 1u);
  auto collector = manager.GetCollectorsForTesting().front();

  const base::UnguessableToken profile_id = base::UnguessableToken::Create();
  EXPECT_TRUE(manager.GetCollectorsEnabledForProfile(profile_id).empty());

  manager.EnableForProfile(profile_id, *collector);
  auto enabled_collectors = manager.GetCollectorsEnabledForProfile(profile_id);
  ASSERT_EQ(enabled_collectors.size(), 1u);
  EXPECT_EQ(enabled_collectors[0], collector);

  manager.DisableForProfile(profile_id, *collector);
  EXPECT_TRUE(manager.GetCollectorsEnabledForProfile(profile_id).empty());
}

TEST_F(DevtoolsDurableMessageCollectorManagerTest,
       GetCollectorsEnabledForProfile) {
  DevtoolsDurableMessageCollectorManager manager;
  mojo::Remote<mojom::DurableMessageCollector> collector_remote1;
  manager.AddCollector(collector_remote1.BindNewPipeAndPassReceiver());
  mojo::Remote<mojom::DurableMessageCollector> collector_remote2;
  manager.AddCollector(collector_remote2.BindNewPipeAndPassReceiver());

  ASSERT_EQ(manager.GetCollectorsForTesting().size(), 2u);
  auto collector1 = manager.GetCollectorsForTesting()[0];
  auto collector2 = manager.GetCollectorsForTesting()[1];

  const base::UnguessableToken profile_id = base::UnguessableToken::Create();
  manager.EnableForProfile(profile_id, *collector1);
  manager.EnableForProfile(profile_id, *collector2);

  EXPECT_THAT(manager.GetCollectorsEnabledForProfile(profile_id),
              UnorderedElementsAre(collector1, collector2));
}

TEST_F(DevtoolsDurableMessageCollectorManagerTest,
       MultipleCollectorsForProfile) {
  DevtoolsDurableMessageCollectorManager manager;
  mojo::Remote<mojom::DurableMessageCollector> collector_remote1;
  manager.AddCollector(collector_remote1.BindNewPipeAndPassReceiver());
  mojo::Remote<mojom::DurableMessageCollector> collector_remote2;
  manager.AddCollector(collector_remote2.BindNewPipeAndPassReceiver());

  ASSERT_EQ(manager.GetCollectorsForTesting().size(), 2u);
  auto collector1 = manager.GetCollectorsForTesting()[0];
  auto collector2 = manager.GetCollectorsForTesting()[1];

  const base::UnguessableToken profile_id = base::UnguessableToken::Create();
  manager.EnableForProfile(profile_id, *collector1);
  manager.EnableForProfile(profile_id, *collector2);

  EXPECT_THAT(manager.GetCollectorsEnabledForProfile(profile_id),
              UnorderedElementsAre(collector1, collector2));

  manager.DisableForProfile(profile_id, *collector1);
  EXPECT_THAT(manager.GetCollectorsEnabledForProfile(profile_id),
              ElementsAre(collector2));
}

TEST_F(DevtoolsDurableMessageCollectorManagerTest, MultipleProfiles) {
  DevtoolsDurableMessageCollectorManager manager;
  mojo::Remote<mojom::DurableMessageCollector> collector_remote1;
  manager.AddCollector(collector_remote1.BindNewPipeAndPassReceiver());
  ASSERT_EQ(manager.GetCollectorsForTesting().size(), 1u);
  auto collector1 = manager.GetCollectorsForTesting().front();
  mojo::Remote<mojom::DurableMessageCollector> collector_remote2;
  manager.AddCollector(collector_remote2.BindNewPipeAndPassReceiver());
  auto collectors = manager.GetCollectorsForTesting();
  ASSERT_EQ(collectors.size(), 2u);
  // Find the collector that's not the first one.
  auto collector2 = *std::find_if(
      collectors.begin(), collectors.end(),
      [&collector1](const DevtoolsDurableMessageCollector* collector) {
        return collector != collector1;
      });

  const base::UnguessableToken profile_id1 = base::UnguessableToken::Create();
  const base::UnguessableToken profile_id2 = base::UnguessableToken::Create();

  manager.EnableForProfile(profile_id1, *collector1);
  manager.EnableForProfile(profile_id2, *collector2);

  EXPECT_THAT(manager.GetCollectorsEnabledForProfile(profile_id1),
              ElementsAre(collector1));
  EXPECT_THAT(manager.GetCollectorsEnabledForProfile(profile_id2),
              ElementsAre(collector2));

  collector_remote1.reset();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return manager.GetCollectorsEnabledForProfile(profile_id1).empty();
  }));
  EXPECT_THAT(manager.GetCollectorsEnabledForProfile(profile_id2),
              ElementsAre(collector2));
}

}  // namespace network
