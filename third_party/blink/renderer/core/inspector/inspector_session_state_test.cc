// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {
using mojom::blink::DevToolsSessionState;
using mojom::blink::DevToolsSessionStatePtr;
using std::unique_ptr;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

// This session object is normally on the browser side; see
// content/browser/devtools/devtools_session.{h,cc}, but here's a minimal
// reimplementation to allow testing without sending data through a Mojo pipe.
class FakeDevToolsSession {
 public:
  void ApplyUpdates(DevToolsSessionStatePtr updates) {
    if (!updates)
      return;
    if (!session_state_cookie_)
      session_state_cookie_ = DevToolsSessionState::New();
    for (auto& entry : updates->entries) {
      if (entry.value.has_value())
        session_state_cookie_->entries.Set(entry.key, std::move(entry.value));
      else
        session_state_cookie_->entries.erase(entry.key);
    }
  }

  DevToolsSessionStatePtr CloneCookie() const {
    return session_state_cookie_.Clone();
  }

  DevToolsSessionStatePtr session_state_cookie_;
};

// The InspectorAgentState abstraction is used to group the
// fields of an agent together, and to connect it to the
// InspectorSessionState instance, from which fields
// may receive their initial state (if we reattach)
// and to which fields send their updates.
// In this test, we use a simple struct rather than
// an agent class (like InspectorPageAgent) with a few fields.
struct AgentWithSimpleFields {
  AgentWithSimpleFields()
      : agent_state_("simple_agent"),
        enabled_(&agent_state_, /*default_value=*/false),
        field1_(&agent_state_, /*default_value=*/0.0),
        multiplier_(&agent_state_, /*default_value=*/1.0),
        counter_(&agent_state_, /*default_value=*/1),
        message_(&agent_state_, /*default_value=*/WTF::String()),
        bytes_(&agent_state_, /*default_value=*/{}) {}

  InspectorAgentState agent_state_;
  InspectorAgentState::Boolean enabled_;
  InspectorAgentState::Double field1_;
  InspectorAgentState::Double multiplier_;
  InspectorAgentState::Integer counter_;
  InspectorAgentState::String message_;
  InspectorAgentState::Bytes bytes_;
};

TEST(InspectorSessionStateTest, SimpleFields) {
  // The browser session (DevToolsSession) remains live while renderer
  // sessions (here we just exercise InspectorSessionState) may come and go.
  FakeDevToolsSession dev_tools_session;

  {  // Renderer session.
    InspectorSessionState session_state(dev_tools_session.CloneCookie());
    AgentWithSimpleFields simple_agent;
    simple_agent.agent_state_.InitFrom(&session_state);

    simple_agent.enabled_.Set(true);
    simple_agent.field1_.Set(11.0);
    simple_agent.multiplier_.Set(42.0);
    simple_agent.counter_.Set(311);
    simple_agent.bytes_.Set({0xde, 0xad, 0xbe, 0xef});

    // Test that Latin1 is handled properly
    simple_agent.message_.Set("\xC7 cedilla");

    EXPECT_EQ(true, simple_agent.enabled_.Get());
    EXPECT_EQ(11.0, simple_agent.field1_.Get());
    EXPECT_EQ(42.0, simple_agent.multiplier_.Get());
    EXPECT_EQ(311, simple_agent.counter_.Get());
    EXPECT_EQ("\xC7 cedilla", simple_agent.message_.Get());
    EXPECT_THAT(simple_agent.bytes_.Get(), ElementsAre(0xde, 0xad, 0xbe, 0xef));

    // Now send the updates back to the browser session.
    dev_tools_session.ApplyUpdates(session_state.TakeUpdates());
  }

  {  // Restore renderer session, verify, then make additional updates.
    InspectorSessionState session_state(dev_tools_session.CloneCookie());
    AgentWithSimpleFields simple_agent;
    simple_agent.agent_state_.InitFrom(&session_state);

    EXPECT_EQ(true, simple_agent.enabled_.Get());
    EXPECT_EQ(11.0, simple_agent.field1_.Get());
    EXPECT_EQ(42.0, simple_agent.multiplier_.Get());
    EXPECT_EQ(311, simple_agent.counter_.Get());
    EXPECT_EQ("\xC7 cedilla", simple_agent.message_.Get());
    EXPECT_THAT(simple_agent.bytes_.Get(), ElementsAre(0xde, 0xad, 0xbe, 0xef));

    simple_agent.enabled_.Set(false);
    simple_agent.multiplier_.Clear();
    simple_agent.field1_.Set(-1.0);
    simple_agent.counter_.Set(312);
    simple_agent.bytes_.Set({1, 2, 3});

    // Now send the updates back to the browser session.
    dev_tools_session.ApplyUpdates(session_state.TakeUpdates());
  }

  {  // Restore renderer session, verify, then clear everything.
    InspectorSessionState session_state(dev_tools_session.CloneCookie());
    AgentWithSimpleFields simple_agent;
    simple_agent.agent_state_.InitFrom(&session_state);

    EXPECT_EQ(false, simple_agent.enabled_.Get());
    EXPECT_EQ(-1.0, simple_agent.field1_.Get());
    EXPECT_EQ(1.0, simple_agent.multiplier_.Get());
    EXPECT_EQ(312, simple_agent.counter_.Get());
    EXPECT_THAT(simple_agent.bytes_.Get(), ElementsAre(1, 2, 3));

    simple_agent.enabled_.Clear();
    simple_agent.multiplier_.Set(1.0);  // default value => clears.
    simple_agent.field1_.Clear();
    simple_agent.counter_.Clear();
    simple_agent.bytes_.Clear();
  }
}

// This agent test struct exercises maps from strings to strings
// and strings to doubles.
struct AgentWithMapFields {
  AgentWithMapFields()
      : agent_state_("map_agents"),
        strings_(&agent_state_, /*default_value=*/WTF::String()),
        doubles_(&agent_state_, /*default_value=*/0.0) {}

  InspectorAgentState agent_state_;
  InspectorAgentState::StringMap strings_;
  InspectorAgentState::DoubleMap doubles_;
};

TEST(InspectorSessionStateTest, MapFields) {
  FakeDevToolsSession dev_tools_session;  // Browser session.

  {  // Renderer session.
    InspectorSessionState session_state(dev_tools_session.CloneCookie());
    AgentWithMapFields maps_agent;
    maps_agent.agent_state_.InitFrom(&session_state);

    EXPECT_TRUE(maps_agent.strings_.IsEmpty());

    maps_agent.strings_.Set("key1", "Hello, world.");
    maps_agent.strings_.Set("key2", WTF::String::FromUTF8("I ❤ Unicode."));

    EXPECT_FALSE(maps_agent.strings_.IsEmpty());

    EXPECT_THAT(maps_agent.strings_.Keys(),
                UnorderedElementsAre("key1", "key2"));
    EXPECT_EQ("Hello, world.", maps_agent.strings_.Get("key1"));
    EXPECT_EQ(WTF::String::FromUTF8("I ❤ Unicode."),
              maps_agent.strings_.Get("key2"));
    EXPECT_TRUE(maps_agent.strings_.Get("key3").IsNull());

    // Now send the updates back to the browser session.
    dev_tools_session.ApplyUpdates(session_state.TakeUpdates());
  }

  {  // Restore renderer session, verify, then make additional updates.
    InspectorSessionState session_state(dev_tools_session.CloneCookie());
    AgentWithMapFields maps_agent;
    maps_agent.agent_state_.InitFrom(&session_state);

    EXPECT_THAT(maps_agent.strings_.Keys(),
                UnorderedElementsAre("key1", "key2"));
    EXPECT_EQ("Hello, world.", maps_agent.strings_.Get("key1"));
    EXPECT_EQ(WTF::String::FromUTF8("I ❤ Unicode."),
              maps_agent.strings_.Get("key2"));
    EXPECT_TRUE(maps_agent.strings_.Get("key3").IsNull());

    maps_agent.strings_.Clear("key1");
    maps_agent.strings_.Set("key2", "updated message for key 2");
    maps_agent.strings_.Set("key3", "new message for key 3");

    // Now send the updates back to the browser session.
    dev_tools_session.ApplyUpdates(session_state.TakeUpdates());
  }

  {  // Restore renderer session and verify.
    InspectorSessionState session_state(dev_tools_session.CloneCookie());
    AgentWithMapFields maps_agent;
    maps_agent.agent_state_.InitFrom(&session_state);

    EXPECT_THAT(maps_agent.strings_.Keys(),
                UnorderedElementsAre("key2", "key3"));
    EXPECT_TRUE(maps_agent.strings_.Get("key1").IsNull());
    EXPECT_EQ("updated message for key 2", maps_agent.strings_.Get("key2"));
    EXPECT_EQ("new message for key 3", maps_agent.strings_.Get("key3"));

    maps_agent.strings_.Clear();

    dev_tools_session.ApplyUpdates(session_state.TakeUpdates());
  }

  // The cookie should be empty since everything is cleared.
  DevToolsSessionStatePtr cookie = dev_tools_session.CloneCookie();
  EXPECT_TRUE(cookie->entries.empty());
}

TEST(InspectorSessionStateTest, MultipleAgents) {
  FakeDevToolsSession dev_tools_session;  // Browser session.

  {  // Renderer session.
    InspectorSessionState session_state(dev_tools_session.CloneCookie());
    AgentWithSimpleFields simple_agent;
    simple_agent.agent_state_.InitFrom(&session_state);
    AgentWithMapFields maps_agent;
    maps_agent.agent_state_.InitFrom(&session_state);

    simple_agent.message_.Set("Hello, world.");
    maps_agent.doubles_.Set("Pi", 3.1415);
    dev_tools_session.ApplyUpdates(session_state.TakeUpdates());
  }

  // Show that the keys for the field values are prefixed with the domain name
  // passed to AgentState so that the stored values won't collide.
  DevToolsSessionStatePtr cookie = dev_tools_session.CloneCookie();
  Vector<WTF::String> keys;
  WTF::CopyKeysToVector(cookie->entries, keys);

  EXPECT_THAT(keys, UnorderedElementsAre("map_agents.1/Pi", "simple_agent.4/"));

  {  // Renderer session, maps_agent clears its fields, and show that it will
    // clear the agent's fields, but no other fields.
    InspectorSessionState session_state(dev_tools_session.CloneCookie());
    AgentWithSimpleFields simple_agent;
    simple_agent.agent_state_.InitFrom(&session_state);
    AgentWithMapFields maps_agent;
    maps_agent.agent_state_.InitFrom(&session_state);
    maps_agent.strings_.Set("foo", "bar");
    maps_agent.agent_state_.ClearAllFields();

    EXPECT_TRUE(maps_agent.doubles_.IsEmpty());
    EXPECT_TRUE(maps_agent.strings_.IsEmpty());
    EXPECT_FALSE(simple_agent.message_.Get().empty());  // other agent.

    dev_tools_session.ApplyUpdates(session_state.TakeUpdates());
  }

  {  // Renderer session, this time the simple agent clears its fields and
    // as a result the session has no more entries (both agents are cleared).
    InspectorSessionState session_state(dev_tools_session.CloneCookie());
    AgentWithSimpleFields simple_agent;
    simple_agent.agent_state_.InitFrom(&session_state);
    simple_agent.agent_state_.ClearAllFields();

    dev_tools_session.ApplyUpdates(session_state.TakeUpdates());
  }
  EXPECT_TRUE(dev_tools_session.CloneCookie()->entries.empty());
}
}  // namespace blink
