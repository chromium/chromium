// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class DevToolsManagerTest : public RenderViewHostImplTestHarness {
 public:
  DevToolsManagerTest() = default;
  ~DevToolsManagerTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    DevToolsManager::ShutdownForTests();
  }

  void TearDown() override {
    DevToolsManager::ShutdownForTests();
    RenderViewHostImplTestHarness::TearDown();
  }
};

TEST_F(DevToolsManagerTest, OnMemoryDump) {
  base::HistogramTester histogram_tester;
  DevToolsManager* manager = DevToolsManager::GetInstance();

  // No sessions initially.
  base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  base::trace_event::ProcessMemoryDump pmd(args);
  manager->OnMemoryDump(args, &pmd);
  // Check allocator dump.
  auto* dump = pmd.GetAllocatorDump("devtools/sessions");
  ASSERT_NE(dump, nullptr);
  base::trace_event::MemoryAllocatorDump::Entry entry(
      base::trace_event::MemoryAllocatorDump::kNameObjectCount,
      base::trace_event::MemoryAllocatorDump::kUnitsObjects, 0u);
  EXPECT_THAT(dump->entries(),
              testing::Contains(testing::Eq(testing::ByRef(entry))));

  // Create a host and attach a client.
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetOrCreateFor(web_contents());

  class TestClient : public DevToolsAgentHostClient {
   public:
    void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                                 base::span<const uint8_t> message) override {}
    void AgentHostClosed(DevToolsAgentHost* agent_host) override {}
  };
  TestClient client;
  agent_host->AttachClient(&client);

  base::trace_event::ProcessMemoryDump pmd2(args);
  manager->OnMemoryDump(args, &pmd2);
  auto* dump2 = pmd2.GetAllocatorDump("devtools/sessions");
  ASSERT_NE(dump2, nullptr);
  base::trace_event::MemoryAllocatorDump::Entry entry2(
      base::trace_event::MemoryAllocatorDump::kNameObjectCount,
      base::trace_event::MemoryAllocatorDump::kUnitsObjects, 1u);
  EXPECT_THAT(dump2->entries(),
              testing::Contains(testing::Eq(testing::ByRef(entry2))));

  agent_host->DetachClient(&client);
  base::trace_event::ProcessMemoryDump pmd3(args);
  manager->OnMemoryDump(args, &pmd3);
  auto* dump3 = pmd3.GetAllocatorDump("devtools/sessions");
  ASSERT_NE(dump3, nullptr);
  EXPECT_THAT(dump3->entries(),
              testing::Contains(testing::Eq(testing::ByRef(entry))));
}

}  // namespace content
