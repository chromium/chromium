// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/file_session_storage.h"

#include <optional>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
using base::test::TestFuture;
}  // namespace

class FileSessionStorageTest : public testing::Test {
 public:
  FileSessionStorageTest() = default;
  FileSessionStorageTest(const FileSessionStorageTest&) = delete;
  FileSessionStorageTest& operator=(const FileSessionStorageTest&) = delete;
  ~FileSessionStorageTest() override = default;

  // testing::Test implementation:
  void SetUp() override { ASSERT_TRUE(directory_.CreateUniqueTempDir()); }

  // Note that this method deliberately returns a new instance on every
  // invocation, to ensure the data is really persisted and not simply kept in
  // memory!
  FileSessionStorage storage() { return FileSessionStorage{GetDir()}; }

  void StoreSession(const base::Value::Dict& information = {}) {
    TestFuture<void> done_signal;
    storage().StoreSession(information, done_signal.GetCallback());
    ASSERT_TRUE(done_signal.Wait());
  }

  void DeleteSession() {
    TestFuture<void> done_signal;
    storage().DeleteSession(done_signal.GetCallback());
    ASSERT_TRUE(done_signal.Wait());
  }

  std::optional<base::Value::Dict> RetrieveSession() {
    TestFuture<std::optional<base::Value::Dict>> done_signal;
    storage().RetrieveSession(done_signal.GetCallback());
    return done_signal.Take();
  }

  bool HasSession() {
    TestFuture<bool> has_session;
    storage().HasSession(has_session.GetCallback());
    return has_session.Get();
  }

 private:
  base::FilePath GetDir() { return directory_.GetPath(); }

  base::test::TaskEnvironment environment_;
  base::ScopedTempDir directory_;
};

TEST_F(FileSessionStorageTest, HasSessionShouldInitiallyBeFalse) {
  EXPECT_FALSE(HasSession());
}

TEST_F(FileSessionStorageTest, HasSessionShouldBeTrueAfterStoringASession) {
  StoreSession();
  EXPECT_TRUE(HasSession());
}

TEST_F(FileSessionStorageTest, HasSessionShouldBeFalseAfterDeletingSession) {
  StoreSession();
  DeleteSession();
  EXPECT_FALSE(HasSession());
}

TEST_F(FileSessionStorageTest,
       RetrieveSessionShouldReturnNullIfNoSessionIsStored) {
  EXPECT_EQ(RetrieveSession(), std::nullopt);
}

TEST_F(FileSessionStorageTest,
       RetrieveSessionShouldReturnStoredSessionInformation) {
  auto session_information =
      base::Value::Dict().Set("stored-key", "stored-value");

  StoreSession(session_information);

  std::optional<base::Value::Dict> result = RetrieveSession();
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result, base::test::IsJson(session_information));
}

}  // namespace remoting
