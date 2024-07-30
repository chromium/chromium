// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

class PageReloadScriptInjectionTest : public testing::Test {
 protected:
  blink::mojom::blink::DevToolsSessionStatePtr session_state_cookie_;
  blink::InspectorAgentState agent_state_;
  blink::InspectorPageAgent::PageReloadScriptInjection injection_;
  blink::InspectorSessionState state_;

 public:
  PageReloadScriptInjectionTest()
      : agent_state_("page"),
        injection_(agent_state_),
        state_(session_state_cookie_.Clone()) {}

  void SetUp() override { agent_state_.InitFrom(&state_); }
};

TEST_F(PageReloadScriptInjectionTest, PromotesScript) {
  blink::KURL url("http://example.com");
  injection_.SetPending("script", url);
  ASSERT_TRUE(injection_.GetScriptForInjection(url).empty());
  injection_.PromoteToLoadOnce();
  ASSERT_EQ(injection_.GetScriptForInjection(url), "script");
  injection_.PromoteToLoadOnce();
  ASSERT_TRUE(injection_.GetScriptForInjection(url).empty());
}

TEST_F(PageReloadScriptInjectionTest, ClearsScript) {
  blink::KURL url("http://example.com");
  injection_.SetPending("script", url);
  injection_.clear();
  injection_.PromoteToLoadOnce();
  ASSERT_TRUE(injection_.GetScriptForInjection(url).empty());

  injection_.SetPending("script", url);
  injection_.PromoteToLoadOnce();
  ASSERT_EQ(injection_.GetScriptForInjection(url), "script");
  injection_.clear();
  ASSERT_TRUE(injection_.GetScriptForInjection(url).empty());
}

TEST_F(PageReloadScriptInjectionTest, ChecksLoaderId) {
  blink::KURL url("http://example.com");
  blink::KURL url2("about:blank");
  injection_.SetPending("script", url);
  injection_.PromoteToLoadOnce();
  ASSERT_TRUE(injection_.GetScriptForInjection(url2).empty());
}
