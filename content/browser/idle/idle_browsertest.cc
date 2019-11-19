// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/idle/idle_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::NiceMock;

namespace content {

namespace {

class MockIdleTimeProvider : public IdleManager::IdleTimeProvider {
 public:
  MockIdleTimeProvider() = default;
  ~MockIdleTimeProvider() override = default;

  MOCK_METHOD1(CalculateIdleState, ui::IdleState(base::TimeDelta));
  MOCK_METHOD0(CalculateIdleTime, base::TimeDelta());
  MOCK_METHOD0(CheckIdleStateIsLocked, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIdleTimeProvider);
};

class IdleTest : public ContentBrowserTest {
 public:
  IdleTest() = default;
  ~IdleTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "IdleDetection");
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(IdleTest, Start) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));

  auto mock_time_provider = std::make_unique<NiceMock<MockIdleTimeProvider>>();
  auto* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  IdleManager* idle_mgr =
      static_cast<StoragePartitionImpl*>(rph->GetStoragePartition())
          ->GetIdleManager();

  // Test that statuses are updated after idleDetector.start().
  std::string script = R"(
    (async () => {
        let idleDetector = new IdleDetector({threshold: 60});
        await idleDetector.start();
        return new Promise(function(resolve) {
          let states = [];
          idleDetector.addEventListener('change', e => {
            let {user, screen} = idleDetector.state;
            states.push(`${user}-${screen}`)
            if (states.length >= 3) {
              let states_str = states.join(',');
              resolve(states_str);
            }
          });
        });
    }) ();
  )";

  EXPECT_CALL(*mock_time_provider, CalculateIdleTime())
      // Initial state of the system.
      .WillOnce(testing::Return(base::TimeDelta::FromSeconds(0)))
      // Simulates a user going idle.
      .WillOnce(testing::Return(base::TimeDelta::FromSeconds(60)))
      // Simulates a screen getting locked after the user goes idle.
      .WillOnce(testing::Return(base::TimeDelta::FromSeconds(60)))
      // Simulates a user going back to active.
      .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(0)));

  EXPECT_CALL(*mock_time_provider, CheckIdleStateIsLocked())
      // Initial state of the system.
      .WillOnce(testing::Return(false))
      // Simulates unlocked screen while user goes idle.
      .WillOnce(testing::Return(false))
      // Simulates a screen getting locked after the user goes idle.
      .WillOnce(testing::Return(true))
      // Simulates an unlocked screen as user goes back to active.
      .WillRepeatedly(testing::Return(false));

  idle_mgr->SetIdleTimeProviderForTest(std::move(mock_time_provider));

  std::string result = EvalJs(shell(), script).ExtractString();
  std::vector<std::string> states = base::SplitString(
      result, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  EXPECT_EQ("idle-unlocked", states.at(0));
  EXPECT_EQ("idle-locked", states.at(1));
  EXPECT_EQ("active-unlocked", states.at(2));
}

}  // namespace content
