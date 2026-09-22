// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/session_routing_config.h"

#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "remoting/base/errors.h"
#include "remoting/base/loggable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr char kValidSessionUserJson[] = R"({
  "sessionUser": "alice"
})";

constexpr char kValidMatchCorpJson[] = R"({
  "matchCorpClientUsername": true,
  "createRemoteUserSessions": true
})";

constexpr char kMutuallyExclusiveJson[] = R"({
  "sessionUser": "alice",
  "matchCorpClientUsername": true
})";

constexpr char kUnknownKeysJson[] = R"({
  "sessionUser": "bob",
  "unknownFutureSetting": "some_value"
})";

constexpr char kMalformedJson[] = R"({
  "sessionUser": "alice",
)";

}  // namespace

class SessionRoutingConfigTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    current_uid_ = geteuid();
  }

 protected:
  base::FilePath CreateConfigFile(const std::string& content,
                                  int permissions = 0600) {
    base::FilePath file_path = temp_dir_.GetPath().Append("test_config.json");
    EXPECT_TRUE(base::WriteFile(file_path, content));
    EXPECT_TRUE(base::SetPosixFilePermissions(file_path, permissions));
    return file_path;
  }

  base::ScopedTempDir temp_dir_;
  uid_t current_uid_ = 0;
};

TEST_F(SessionRoutingConfigTest, MissingFileReturnsNullopt) {
  base::FilePath non_existent =
      temp_dir_.GetPath().Append("does_not_exist.json");
  auto result = SessionRoutingConfig::LoadAndValidate(
      non_existent, /*is_corp_host=*/false, current_uid_);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->has_value());
}

TEST_F(SessionRoutingConfigTest, SymlinkFails) {
  base::FilePath target_file = CreateConfigFile(kValidSessionUserJson);
  base::FilePath symlink_file = temp_dir_.GetPath().Append("symlink.json");
  ASSERT_TRUE(base::CreateSymbolicLink(target_file, symlink_file));

  auto result = SessionRoutingConfig::LoadAndValidate(
      symlink_file, /*is_corp_host=*/false, current_uid_);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SessionRoutingConfigTest, DirectoryFails) {
  base::FilePath dir_path = temp_dir_.GetPath().Append("sub_dir");
  ASSERT_TRUE(base::CreateDirectory(dir_path));

  auto result = SessionRoutingConfig::LoadAndValidate(
      dir_path, /*is_corp_host=*/false, current_uid_);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SessionRoutingConfigTest, WrongOwnerFails) {
  base::FilePath config_file = CreateConfigFile(kValidSessionUserJson);
  // Expect a different UID.
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_ + 1);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SessionRoutingConfigTest, GroupWritableFails) {
  base::FilePath config_file = CreateConfigFile(kValidSessionUserJson, 0620);
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SessionRoutingConfigTest, WorldWritableFails) {
  base::FilePath config_file = CreateConfigFile(kValidSessionUserJson, 0666);
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SessionRoutingConfigTest, ValidPermissionsPass) {
  // 0600
  base::FilePath config_file = CreateConfigFile(kValidSessionUserJson, 0600);
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(result->value().mode(),
            SessionRoutingConfig::RoutingMode::kSessionUser);
  EXPECT_EQ(result->value().session_user(), "alice");

  // 0644
  ASSERT_TRUE(base::SetPosixFilePermissions(config_file, 0644));
  result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
}

TEST_F(SessionRoutingConfigTest, FileSizeExceededFails) {
  // Config exceeding 64 KB limit.
  std::string oversized_content =
      "{\"sessionUser\": \"alice\", \"padding\": \"";
  oversized_content.append(65 * 1024, 'x');
  oversized_content += "\"}";

  base::FilePath config_file = CreateConfigFile(oversized_content);
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SessionRoutingConfigTest, MalformedJsonFails) {
  base::FilePath config_file = CreateConfigFile(kMalformedJson);
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SessionRoutingConfigTest, NonDictJsonFails) {
  base::FilePath config_file = CreateConfigFile("[\"alice\"]");
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SessionRoutingConfigTest, InvalidFieldTypesFail) {
  // sessionUser not a string
  base::FilePath config_file1 = CreateConfigFile(R"({"sessionUser": 12345})");
  EXPECT_FALSE(SessionRoutingConfig::LoadAndValidate(
                   config_file1, /*is_corp_host=*/false, current_uid_)
                   .has_value());

  // matchCorpClientUsername not a bool
  base::FilePath config_file2 =
      CreateConfigFile(R"({"matchCorpClientUsername": "true"})");
  EXPECT_FALSE(SessionRoutingConfig::LoadAndValidate(
                   config_file2, /*is_corp_host=*/false, current_uid_)
                   .has_value());

  // createRemoteUserSessions not a bool
  base::FilePath config_file3 =
      CreateConfigFile(R"({"createRemoteUserSessions": 1})");
  EXPECT_FALSE(SessionRoutingConfig::LoadAndValidate(
                   config_file3, /*is_corp_host=*/false, current_uid_)
                   .has_value());
}

TEST_F(SessionRoutingConfigTest, MutuallyExclusiveFieldsFail) {
  base::FilePath config_file = CreateConfigFile(kMutuallyExclusiveJson);
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SessionRoutingConfigTest, InvalidUsernameFails) {
  // Empty username
  base::FilePath config_file1 = CreateConfigFile(R"({"sessionUser": ""})");
  EXPECT_FALSE(SessionRoutingConfig::LoadAndValidate(
                   config_file1, /*is_corp_host=*/false, current_uid_)
                   .has_value());

  // Username starting with hyphen
  base::FilePath config_file2 =
      CreateConfigFile(R"({"sessionUser": "-baduser"})");
  EXPECT_FALSE(SessionRoutingConfig::LoadAndValidate(
                   config_file2, /*is_corp_host=*/false, current_uid_)
                   .has_value());

  // Username with forbidden characters
  base::FilePath config_file3 =
      CreateConfigFile(R"({"sessionUser": "user/name"})");
  EXPECT_FALSE(SessionRoutingConfig::LoadAndValidate(
                   config_file3, /*is_corp_host=*/false, current_uid_)
                   .has_value());
}

TEST_F(SessionRoutingConfigTest, UnknownKeysIgnoredWithWarning) {
  base::FilePath config_file = CreateConfigFile(kUnknownKeysJson);
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(result->value().mode(),
            SessionRoutingConfig::RoutingMode::kSessionUser);
  EXPECT_EQ(result->value().session_user(), "bob");
}

TEST_F(SessionRoutingConfigTest, EmptyDictSetsRejectAll) {
  base::FilePath config_file = CreateConfigFile("{}");
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/false, current_uid_);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(result->value().mode(),
            SessionRoutingConfig::RoutingMode::kRejectAll);
  EXPECT_TRUE(result->value().session_user().empty());
  EXPECT_FALSE(result->value().create_remote_user_sessions());

  // Resolving any client rejects
  auto user = result->value().ResolveSessionUser("alice@google.com");
  EXPECT_FALSE(user.has_value());
  EXPECT_EQ(user.error(), ErrorCode::SESSION_REJECTED);
}

TEST_F(SessionRoutingConfigTest, MatchCorpClientUsernameParsed) {
  base::FilePath config_file = CreateConfigFile(kValidMatchCorpJson);
  auto result = SessionRoutingConfig::LoadAndValidate(
      config_file, /*is_corp_host=*/true, current_uid_);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(result->value().mode(),
            SessionRoutingConfig::RoutingMode::kMatchCorpClientUsername);
  EXPECT_TRUE(result->value().create_remote_user_sessions());
  EXPECT_TRUE(result->value().is_corp_host());
}

TEST_F(SessionRoutingConfigTest, ResolveSessionUser_SessionUserMode) {
  SessionRoutingConfig config(SessionRoutingConfig::RoutingMode::kSessionUser,
                              "alice",
                              /*create_remote_user_sessions=*/false);

  auto resolved1 = config.ResolveSessionUser("anyone@google.com");
  ASSERT_TRUE(resolved1.has_value());
  EXPECT_EQ(*resolved1, "alice");

  auto resolved2 = config.ResolveSessionUser("anyone@gmail.com/resource");
  ASSERT_TRUE(resolved2.has_value());
  EXPECT_EQ(*resolved2, "alice");
}

TEST_F(SessionRoutingConfigTest, ResolveSessionUser_MatchCorpMode_CorpHost) {
  SessionRoutingConfig config(
      SessionRoutingConfig::RoutingMode::kMatchCorpClientUsername,
      /*session_user=*/{}, /*create_remote_user_sessions=*/false,
      /*is_corp_host=*/true);

  // Standard corp user
  auto resolved1 = config.ResolveSessionUser("john.doe@google.com");
  ASSERT_TRUE(resolved1.has_value());
  EXPECT_EQ(*resolved1, "john.doe");

  // Silo domain user on a corp host
  auto resolved_silo = config.ResolveSessionUser("jane.doe@waymo.com");
  ASSERT_TRUE(resolved_silo.has_value());
  EXPECT_EQ(*resolved_silo, "jane.doe");

  // Uppercase in email is converted to lowercase
  auto resolved2 = config.ResolveSessionUser("John.Doe@SiloDomain.com");
  ASSERT_TRUE(resolved2.has_value());
  EXPECT_EQ(*resolved2, "john.doe");

  // With resource suffix
  auto resolved3 =
      config.ResolveSessionUser("charlie@google.com/chromoting_client_123");
  ASSERT_TRUE(resolved3.has_value());
  EXPECT_EQ(*resolved3, "charlie");

  // Malformed emails with multiple '@' or missing domain/username are rejected
  EXPECT_FALSE(config.ResolveSessionUser("user@domain@extra.com").has_value());
  EXPECT_FALSE(config.ResolveSessionUser("user@").has_value());
  EXPECT_FALSE(config.ResolveSessionUser("@domain.com").has_value());
}

TEST_F(SessionRoutingConfigTest, ResolveSessionUser_MatchCorpMode_NonCorpHost) {
  // When is_corp_host is false, it falls back to IsGoogleEmail.
  SessionRoutingConfig config(
      SessionRoutingConfig::RoutingMode::kMatchCorpClientUsername,
      /*session_user=*/{}, /*create_remote_user_sessions=*/false,
      /*is_corp_host=*/false);

  // Standard Google email succeeds
  auto resolved1 = config.ResolveSessionUser("john.doe@google.com");
  ASSERT_TRUE(resolved1.has_value());
  EXPECT_EQ(*resolved1, "john.doe");

  // Silo domain is rejected on a non-corp host
  auto resolved_silo = config.ResolveSessionUser("jane.doe@waymo.com");
  EXPECT_FALSE(resolved_silo.has_value());
  EXPECT_EQ(resolved_silo.error(), ErrorCode::SESSION_REJECTED);

  // Consumer email is rejected on a non-corp host
  auto non_corp = config.ResolveSessionUser("john.doe@gmail.com");
  EXPECT_FALSE(non_corp.has_value());
  EXPECT_EQ(non_corp.error(), ErrorCode::SESSION_REJECTED);
}

}  // namespace remoting
