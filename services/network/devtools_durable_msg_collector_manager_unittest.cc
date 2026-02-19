// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/devtools_durable_msg_collector_manager.h"

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

using testing::ElementsAre;
using testing::UnorderedElementsAre;

class DevtoolsDurableMessageCollectorManagerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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

TEST_F(DevtoolsDurableMessageCollectorManagerTest, ReportAggregateMemoryUsage) {
  using base::trace_event::MemoryAllocatorDump;
  using testing::ByRef;
  using testing::Contains;
  using testing::Eq;

  DevtoolsDurableMessageCollectorManager manager;
  mojo::Remote<mojom::DurableMessageCollector> collector_remote;
  manager.AddCollector(collector_remote.BindNewPipeAndPassReceiver());
  auto collectors = manager.GetCollectorsForTesting();
  ASSERT_EQ(collectors.size(), 1u);

  auto collector = collectors.front();
  collector->Configure(
      mojom::NetworkDurableMessageConfig::New(10 * 1024 * 1024),
      base::DoNothing());

  auto durable_message = collector->CreateDurableMessage("request_id1");
  const std::string message_str(5 * 1024 * 1024, 'a');
  const auto message = base::as_byte_span(message_str);
  durable_message->AddBytes(message, message.size());

  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  base::trace_event::ProcessMemoryDump pmd(args);
  ASSERT_TRUE(manager.OnMemoryDump(args, &pmd));
  auto* dump = pmd.GetAllocatorDump("devtools/durable_message_collectors");
  ASSERT_NE(dump, nullptr);
  MemoryAllocatorDump::Entry entry("size", "bytes", 5 * 1024 * 1024u);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(entry))));
  MemoryAllocatorDump::Entry object_count_entry(
      MemoryAllocatorDump::kNameObjectCount, MemoryAllocatorDump::kUnitsObjects,
      1u);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(object_count_entry))));
  MemoryAllocatorDump::Entry entry_count("object_count", "objects", 1u);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(entry_count))));

  collector_remote.reset();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager.GetCollectorsForTesting().empty(); }));

  base::trace_event::ProcessMemoryDump pmd2(args);
  ASSERT_TRUE(manager.OnMemoryDump(args, &pmd2));
  auto* dump2 = pmd2.GetAllocatorDump("devtools/durable_message_collectors");
  // Implementation creates dump always.
  ASSERT_NE(dump2, nullptr);
  MemoryAllocatorDump::Entry entry2("size", "bytes", 0u);
  EXPECT_THAT(dump2->entries(), Contains(Eq(ByRef(entry2))));
  MemoryAllocatorDump::Entry entry2_count("object_count", "objects", 0u);
  EXPECT_THAT(dump2->entries(), Contains(Eq(ByRef(entry2_count))));
}

TEST_F(DevtoolsDurableMessageCollectorManagerTest, GlobalLimit) {
  // Set a global limit of 15 MiB.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      network::features::kDurableMessages,
      {{network::features::kDurableMessagesGlobalBufferSize.name,
        "15728640"}});  // 15 * 1024 * 1024

  DevtoolsDurableMessageCollectorManager manager;

  // Add first collector
  mojo::Remote<mojom::DurableMessageCollector> collector_remote1;
  manager.AddCollector(collector_remote1.BindNewPipeAndPassReceiver());
  const base::UnguessableToken profile_id = base::UnguessableToken::Create();

  auto collectors = manager.GetCollectorsForTesting();
  ASSERT_EQ(collectors.size(), 1u);
  auto collector1 = collectors.front();
  manager.EnableForProfile(profile_id, *collector1);
  collector1->Configure(
      mojom::NetworkDurableMessageConfig::New(10 * 1024 * 1024),
      base::DoNothing());

  // Add 10 MiB to the first collector (should succeed as global limit is 15
  // MiB)
  auto durable_message1 = collector1->CreateDurableMessage("request_id1");
  const std::string message_str1(10 * 1024 * 1024, 'a');
  durable_message1->AddBytes(base::as_byte_span(message_str1),
                             message_str1.size());
  EXPECT_EQ(manager.total_memory_usage_for_testing(), 10 * 1024 * 1024u);

  // Add second collector
  mojo::Remote<mojom::DurableMessageCollector> collector_remote2;
  manager.AddCollector(collector_remote2.BindNewPipeAndPassReceiver());

  collectors = manager.GetCollectorsForTesting();
  ASSERT_EQ(collectors.size(), 2u);
  auto collector2 = *std::find_if(
      collectors.begin(), collectors.end(),
      [&collector1](const DevtoolsDurableMessageCollector* collector) {
        return collector != collector1;
      });
  manager.EnableForProfile(profile_id, *collector2);
  collector2->Configure(
      mojom::NetworkDurableMessageConfig::New(10 * 1024 * 1024),
      base::DoNothing());

  // Add 4 MiB to the second collector (should succeed, total = 14 MiB <= 15
  // MiB)
  auto durable_message2 = collector2->CreateDurableMessage("request_id2");
  const std::string message_str2(4 * 1024 * 1024, 'b');
  durable_message2->AddBytes(base::as_byte_span(message_str2),
                             message_str2.size());
  EXPECT_EQ(manager.total_memory_usage_for_testing(), 14 * 1024 * 1024u);

  // Add 6 MiB to the second collector
  // The global limit is 15 MiB, current global usage is 14 MiB.
  // Expected: collector2 tries to add 6 MiB. manager->CanAccommodate(6MB)
  // returns false (14+6 > 15). collector2 evicts its 4MB message. Now total is
  // 10MB. manager->CanAccommodate(6MB) returns false (10+6 > 15). collector2
  // drops the 6MB addition because it still can't accommodate it globally.
  auto durable_message3 = collector2->CreateDurableMessage("request_id3");
  const std::string message_str3(6 * 1024 * 1024, 'c');
  durable_message3->AddBytes(base::as_byte_span(message_str3),
                             message_str3.size());

  // The 4MB message was evicted. The 6MB message was dropped.
  // Total usage should be 10MB (from the first collector).
  EXPECT_EQ(manager.total_memory_usage_for_testing(), 10 * 1024 * 1024u);

  collector_remote1.reset();
  collector_remote2.reset();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager.GetCollectorsForTesting().empty(); }));
}

}  // namespace network
