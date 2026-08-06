// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_session_manager.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
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

  void StartManager() {
    manager_.Start(output_callback_.Get(), exit_callback_.Get());
    base::ThreadPoolInstance::Get()->FlushForTesting();
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  TerminalSessionManager manager_;
  base::MockCallback<TerminalSessionManager::OutputCallback> output_callback_;
  base::MockCallback<TerminalSessionManager::ExitCallback> exit_callback_;
};

TEST_F(TerminalSessionManagerTest, CreateTerminalAndAssignsId) {
  StartManager();
  int32_t id = manager_.CreateTerminal();
  int32_t id2 = manager_.CreateTerminal();
  int32_t id3 = manager_.CreateTerminal();
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
  StartManager();
  FakeTerminalSession::SetNextStartFail(true);
  int32_t id = manager_.CreateTerminal();
  EXPECT_EQ(id, -1);
  EXPECT_TRUE(manager_.GetTerminalSessionIds().empty());
}

TEST_F(TerminalSessionManagerTest, WriteTerminalRoutesCorrectly) {
  StartManager();
  int32_t id = manager_.CreateTerminal();
  ASSERT_EQ(id, 1);

  auto sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_NE(sessions[0], nullptr);

  manager_.WriteTerminal(id, "hello");
  EXPECT_EQ(sessions[0]->inputs().size(), 1u);
  EXPECT_EQ(sessions[0]->inputs()[0], "hello");
}

TEST_F(TerminalSessionManagerTest, ResizeTerminalRoutesCorrectly) {
  StartManager();
  int32_t id = manager_.CreateTerminal();
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
  StartManager();
  int32_t id = manager_.CreateTerminal();
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
  StartManager();
  EXPECT_TRUE(manager_.GetTerminalSessionIds().empty());

  int32_t id1 = manager_.CreateTerminal();
  int32_t id2 = manager_.CreateTerminal();

  std::vector<int32_t> ids = manager_.GetTerminalSessionIds();
  ASSERT_EQ(ids.size(), 2u);
  EXPECT_EQ(ids[0], id1);
  EXPECT_EQ(ids[1], id2);
}

TEST_F(TerminalSessionManagerTest, DetachAllSessionsDetachesSessions) {
  StartManager();
  int32_t id = manager_.CreateTerminal();
  ASSERT_EQ(id, 1);

  auto sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_NE(sessions[0], nullptr);

  manager_.DetachAllSessions();

  // Terminal session should be removed from the manager.
  EXPECT_EQ(manager_.GetTerminalSession(id), nullptr);
  EXPECT_TRUE(manager_.GetTerminalSessionIds().empty());

  // Terminal sessions list mock should be empty as FakeTerminalSession's
  // destructor is called.
  EXPECT_TRUE(FakeTerminalSession::GetActiveSessions().empty());

  // Since we called Detach(), it should NOT have been terminated.
  EXPECT_FALSE(FakeTerminalSession::WasTerminated(id));
}

TEST_F(TerminalSessionManagerTest, RestorePersistentTerminalsWithoutCollision) {
  FakeTerminalSession::SetPersistentTerminalIds({20, -1, 10});

  // Restore persistent terminals asynchronously via Start().
  manager_.Start(output_callback_.Get(), exit_callback_.Get());
  ASSERT_TRUE(base::test::RunUntil(
      [this] { return manager_.GetTerminalSessionIds().size() == 2u; }));

  // Restored persistent terminals should exist in sorted order.
  EXPECT_THAT(manager_.GetTerminalSessionIds(),
              testing::ElementsAre(10, 20));
  EXPECT_NE(manager_.GetTerminalSession(10), nullptr);
  EXPECT_NE(manager_.GetTerminalSession(20), nullptr);

  int32_t post_restore_id = manager_.CreateTerminal();
  EXPECT_EQ(post_restore_id, 21);
}

TEST_F(TerminalSessionManagerTest,
       DetachAllSessionsCancelsPendingRestoration) {
  FakeTerminalSession::SetPersistentTerminalIds({10, 20});

  // Restore persistent terminals asynchronously via Start().
  manager_.Start(output_callback_.Get(), exit_callback_.Get());

  // Immediately disconnect the client before tasks can complete.
  manager_.DetachAllSessions();

  // Block until all background ThreadPool tasks have completed, then drain any
  // posted replies on the origin sequence.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  task_environment_.RunUntilIdle();

  // No terminal sessions should have been restored because weak pointers were
  // invalidated on disconnect.
  EXPECT_TRUE(manager_.GetTerminalSessionIds().empty());
}

TEST_F(TerminalSessionManagerTest, CreateTerminalFailsDuringRestore) {
  FakeTerminalSession::SetPersistentTerminalIds({10, 20});

  manager_.Start(output_callback_.Get(), exit_callback_.Get());

  // Calling CreateTerminal while restore is in flight should return -1.
  EXPECT_EQ(manager_.CreateTerminal(), -1);

  ASSERT_TRUE(base::test::RunUntil(
      [this] { return manager_.GetTerminalSessionIds().size() == 2u; }));

  // After restoration completes, CreateTerminal should succeed without collision.
  int32_t post_restore_id = manager_.CreateTerminal();
  EXPECT_EQ(post_restore_id, 21);
}

}  // namespace remoting
