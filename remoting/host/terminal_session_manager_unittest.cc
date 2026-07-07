// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_session_manager.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "remoting/host/fake_terminal_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class TerminalSessionManagerTest : public testing::Test {
 protected:
  TerminalSessionManagerTest() = default;
  ~TerminalSessionManagerTest() override = default;

  // Resets the static state of FakeTerminalSession.
  void SetUp() override { FakeTerminalSession::ResetStaticState(); }

  TerminalSessionManager manager_;
  base::MockCallback<TerminalSessionManager::OutputCallback> output_callback_;
  base::MockCallback<TerminalSessionManager::ExitCallback> exit_callback_;
};

TEST_F(TerminalSessionManagerTest, CreateTerminalAndAssignsId) {
  int32_t id =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  int32_t id2 =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  int32_t id3 =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  ASSERT_EQ(id, 1);
  ASSERT_EQ(id2, 2);
  ASSERT_EQ(id3, 3);

  auto sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 3u);
  ASSERT_NE(sessions[0], nullptr);
  ASSERT_NE(sessions[1], nullptr);
  ASSERT_NE(sessions[2], nullptr);

  EXPECT_EQ(manager_.GetTerminalSession(id), sessions[0].get());
  EXPECT_EQ(manager_.GetTerminalSession(id2), sessions[1].get());
  EXPECT_EQ(manager_.GetTerminalSession(id3), sessions[2].get());
}

TEST_F(TerminalSessionManagerTest, StartFailureReturnsMinusOne) {
  FakeTerminalSession::SetNextStartFail(true);
  int32_t id =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  EXPECT_EQ(id, -1);
}

TEST_F(TerminalSessionManagerTest, WriteTerminalRoutesCorrectly) {
  int32_t id =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  ASSERT_EQ(id, 1);

  auto sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_NE(sessions[0], nullptr);

  manager_.WriteTerminal(id, "hello");
  EXPECT_EQ(sessions[0]->inputs().size(), 1u);
  EXPECT_EQ(sessions[0]->inputs()[0], "hello");
}

TEST_F(TerminalSessionManagerTest, ResizeTerminalRoutesCorrectly) {
  int32_t id =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  ASSERT_EQ(id, 1);

  auto sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_NE(sessions[0], nullptr);

  manager_.ResizeTerminal(id, 80, 24);
  EXPECT_EQ(sessions[0]->resizes().size(), 1u);
  EXPECT_EQ(sessions[0]->resizes()[0].first, 80u);
  EXPECT_EQ(sessions[0]->resizes()[0].second, 24u);
}

TEST_F(TerminalSessionManagerTest, CloseTerminalDestroysAndTerminates) {
  int32_t id =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  ASSERT_EQ(id, 1);

  auto sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_NE(sessions[0], nullptr);

  manager_.CloseTerminal(id);
  EXPECT_TRUE(FakeTerminalSession::WasTerminated(id));
  EXPECT_TRUE(FakeTerminalSession::GetActiveSessions().empty());
  EXPECT_EQ(manager_.GetTerminalSession(id), nullptr);
}

TEST_F(TerminalSessionManagerTest, GetTerminalSessionAndIds) {
  EXPECT_TRUE(manager_.GetTerminalSessionIds().empty());

  int32_t id1 =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  int32_t id2 =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());

  std::vector<int32_t> ids = manager_.GetTerminalSessionIds();
  ASSERT_EQ(ids.size(), 2u);
  EXPECT_EQ(ids[0], id1);
  EXPECT_EQ(ids[1], id2);
}

}  // namespace remoting
