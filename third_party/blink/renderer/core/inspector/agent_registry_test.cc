// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/agent_registry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class TestingAgent final : public GarbageCollected<TestingAgent> {
 public:
  TestingAgent() = default;
  void Trace(Visitor* visitor) const {}
};

TEST(AgentRegistryTest, AddRemove) {
  AgentRegistry<TestingAgent> testing_agents = AgentRegistry<TestingAgent>();
  Persistent<TestingAgent> agent = MakeGarbageCollected<TestingAgent>();
  testing_agents.AddAgent(agent);
  EXPECT_EQ(testing_agents.size(), 1u);
  testing_agents.RemoveAgent(agent);
  EXPECT_EQ(testing_agents.size(), 0u);
}

TEST(AgentRegistryTest, Duplicate) {
  AgentRegistry<TestingAgent> testing_agents = AgentRegistry<TestingAgent>();
  Persistent<TestingAgent> agent = MakeGarbageCollected<TestingAgent>();
  testing_agents.AddAgent(agent);
  testing_agents.AddAgent(agent);
  EXPECT_EQ(testing_agents.size(), 1u);
  testing_agents.RemoveAgent(agent);
  EXPECT_EQ(testing_agents.size(), 0u);
}

TEST(AgentRegistryTest, IteratingOverAgents) {
  AgentRegistry<TestingAgent> testing_agents = AgentRegistry<TestingAgent>();
  Persistent<TestingAgent> agent = MakeGarbageCollected<TestingAgent>();
  testing_agents.AddAgent(agent);
  EXPECT_FALSE(testing_agents.RequiresCopy());
  testing_agents.ForEachAgent(
      [&](TestingAgent* agent) { EXPECT_TRUE(testing_agents.RequiresCopy()); });
}

TEST(AgentRegistryTest, ModificationDuringIteration) {
  AgentRegistry<TestingAgent> testing_agents = AgentRegistry<TestingAgent>();
  Persistent<TestingAgent> agent1 = MakeGarbageCollected<TestingAgent>();
  Persistent<TestingAgent> agent2 = MakeGarbageCollected<TestingAgent>();
  Persistent<TestingAgent> agent3 = MakeGarbageCollected<TestingAgent>();
  testing_agents.AddAgent(agent1);
  testing_agents.AddAgent(agent2);
  testing_agents.AddAgent(agent3);
  EXPECT_FALSE(testing_agents.RequiresCopy());
  testing_agents.ForEachAgent([&](TestingAgent* agent) {
    if (agent == agent1)
      EXPECT_TRUE(testing_agents.RequiresCopy());
    else
      EXPECT_FALSE(testing_agents.RequiresCopy());
    testing_agents.RemoveAgent(agent);
    if (agent == agent3)
      testing_agents.AddAgent(agent1);
  });
  EXPECT_EQ(testing_agents.size(), 1u);
}

}  // namespace blink
