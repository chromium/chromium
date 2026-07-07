// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_session_manager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "remoting/host/terminal_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace remoting {

namespace {

using CreateMockSessionCallback =
    base::RepeatingCallback<std::unique_ptr<TerminalSession>(
        TerminalSessionManager::OutputCallback,
        TerminalSessionManager::ExitCallback,
        int32_t)>;

CreateMockSessionCallback* g_mock_session_creator = nullptr;

class MockTerminalSession : public TerminalSession {
 public:
  MockTerminalSession() = default;
  ~MockTerminalSession() override = default;

  MOCK_METHOD(bool, Start, (), (override));
  MOCK_METHOD(void, Write, (const std::string& data), (override));
  MOCK_METHOD(void, Resize, (uint32_t width, uint32_t height), (override));
  MOCK_METHOD(void, Terminate, (), (override));

  base::WeakPtr<MockTerminalSession> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockTerminalSession> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<TerminalSession> TerminalSession::Create(
    TerminalSessionManager::OutputCallback output_cb,
    TerminalSessionManager::ExitCallback exit_cb,
    int32_t id) {
  if (g_mock_session_creator) {
    return g_mock_session_creator->Run(std::move(output_cb), std::move(exit_cb),
                                       id);
  }
  return nullptr;
}

class TerminalSessionManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    creator_callback_ = base::BindRepeating(
        &TerminalSessionManagerTest::CreateMockSession, base::Unretained(this));
    g_mock_session_creator = &creator_callback_;
  }

  void TearDown() override { g_mock_session_creator = nullptr; }

  std::unique_ptr<TerminalSession> CreateMockSession(
      TerminalSessionManager::OutputCallback output_cb,
      TerminalSessionManager::ExitCallback exit_cb,
      int32_t id) {
    auto session = std::make_unique<MockTerminalSession>();
    EXPECT_CALL(*session, Start()).WillOnce(Return(true));
    last_mock_session_ = session->GetWeakPtr();
    mock_sessions_[id] = last_mock_session_;
    return session;
  }

  TerminalSessionManager manager_;
  base::WeakPtr<MockTerminalSession> last_mock_session_;
  std::map<int32_t, base::WeakPtr<MockTerminalSession>> mock_sessions_;
  CreateMockSessionCallback creator_callback_;
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
  ASSERT_NE(mock_sessions_[id], nullptr);
  ASSERT_NE(mock_sessions_[id2], nullptr);
  ASSERT_NE(mock_sessions_[id3], nullptr);
  EXPECT_EQ(manager_.GetTerminalSession(id), mock_sessions_[id].get());
  EXPECT_EQ(manager_.GetTerminalSession(id2), mock_sessions_[id2].get());
  EXPECT_EQ(manager_.GetTerminalSession(id3), mock_sessions_[id3].get());
}

TEST_F(TerminalSessionManagerTest, StartFailureReturnsMinusOne) {
  auto local_creator =
      base::BindRepeating([](TerminalSessionManager::OutputCallback output_cb,
                             TerminalSessionManager::ExitCallback exit_cb,
                             int32_t id) -> std::unique_ptr<TerminalSession> {
        auto mock = std::make_unique<MockTerminalSession>();
        EXPECT_CALL(*mock, Start()).WillOnce(Return(false));
        return mock;
      });
  g_mock_session_creator = &local_creator;

  int32_t id =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  EXPECT_EQ(id, -1);
}

TEST_F(TerminalSessionManagerTest, WriteTerminalRoutesCorrectly) {
  int32_t id =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  ASSERT_EQ(id, 1);
  ASSERT_NE(last_mock_session_, nullptr);

  EXPECT_CALL(*last_mock_session_, Write("hello")).Times(1);
  manager_.WriteTerminal(id, "hello");
}

TEST_F(TerminalSessionManagerTest, ResizeTerminalRoutesCorrectly) {
  int32_t id =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  ASSERT_EQ(id, 1);
  ASSERT_NE(last_mock_session_, nullptr);

  EXPECT_CALL(*last_mock_session_, Resize(80, 24)).Times(1);
  manager_.ResizeTerminal(id, 80, 24);
}

TEST_F(TerminalSessionManagerTest, CloseTerminalDestroysAndTerminates) {
  int32_t id =
      manager_.CreateTerminal(output_callback_.Get(), exit_callback_.Get());
  ASSERT_EQ(id, 1);
  ASSERT_NE(last_mock_session_, nullptr);

  EXPECT_CALL(*last_mock_session_, Terminate()).Times(1);
  manager_.CloseTerminal(id);

  EXPECT_EQ(last_mock_session_, nullptr);
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
