// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/remoting_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/host/chromeos/file_session_storage.h"
#include "remoting/host/chromeos/remote_support_host_ash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using base::test::TestFuture;

class RemotingServiceTest : public ::testing::Test {
 public:
  RemotingServiceTest() = default;
  RemotingServiceTest(const RemotingServiceTest&) = delete;
  RemotingServiceTest& operator=(const RemotingServiceTest&) = delete;
  ~RemotingServiceTest() override = default;

  // testing::Test implementation:
  void SetUp() override {
    ASSERT_TRUE(session_storage_directory_.CreateUniqueTempDir());

    remoting_service().SetSessionStorageDirectoryForTesting(
        session_storage_directory_.GetPath());
  }

  RemotingService& remoting_service() { return RemotingService::Get(); }

  void CreateReconnectableSession() {
    FileSessionStorage storage(session_storage_directory_.GetPath());

    TestFuture<void> future;
    storage.StoreSession(base::Value::Dict{}, future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void EnsureNoReconnectableSession() {
    FileSessionStorage storage(session_storage_directory_.GetPath());

    TestFuture<void> future;
    storage.DeleteSession(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

 private:
  base::test::TaskEnvironment environment_;
  base::ScopedTempDir session_storage_directory_;
};

TEST_F(RemotingServiceTest,
       ShouldReturnNoSessionIdIdIfThereIsNoReconnectableSession) {
  EnsureNoReconnectableSession();

  TestFuture<std::optional<SessionId>> future;
  remoting_service().GetReconnectableEnterpriseSessionId(future.GetCallback());
  std::optional<SessionId> result = future.Take();

  EXPECT_EQ(result, std::nullopt);
}

TEST_F(RemotingServiceTest, ShouldReturnEnterpriseSessionIdIfSessionIsStored) {
  CreateReconnectableSession();

  TestFuture<std::optional<SessionId>> future;
  remoting_service().GetReconnectableEnterpriseSessionId(future.GetCallback());
  std::optional<SessionId> result = future.Take();

  EXPECT_EQ(result, kEnterpriseSessionId);
}

}  // namespace remoting
