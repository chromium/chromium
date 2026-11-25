// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_linux.h"

#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "remoting/base/crash/crash_reporting_crashpad.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace remoting {

namespace {

constexpr char kValidOverrideConfigJson[] =
    R"({"upload_url": "https://clients2.google.com/cr/staging_report", )"
    R"("disable_rate_limit": true})";

}  // namespace

TEST(CrashpadLinuxTest, InitializeClientWithInvalidFd) {
  EXPECT_FALSE(CrashpadLinux::InitializeClient(base::ScopedFD(), getpid()));
}

TEST(CrashpadLinuxTest, GetHandlerSocketBeforeInit) {
  base::ScopedFD sock;
  pid_t pid = -1;
  EXPECT_FALSE(CrashpadLinux::GetHandlerSocket(sock, pid));
  EXPECT_FALSE(sock.is_valid());
}

MULTIPROCESS_TEST_MAIN(InitializeClientAndGettersChild) {
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) != 0) {
    return 1;
  }
  base::ScopedFD client_fd(fds[0]);
  base::ScopedFD server_fd(fds[1]);

  pid_t test_pid = 12345;
  if (!InitializeCrashpadClient(std::move(client_fd), test_pid)) {
    return 2;
  }

  base::ScopedFD retrieved_sock;
  pid_t retrieved_pid = -1;
  if (!GetCrashpadHandlerSocket(retrieved_sock, retrieved_pid) ||
      !retrieved_sock.is_valid() || retrieved_pid != test_pid) {
    return 3;
  }

  return 0;
}

TEST(CrashpadLinuxTest, InitializeClientAndGetters) {
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());
  base::Process test_child_process = base::SpawnMultiProcessTestChild(
      "InitializeClientAndGettersChild", command_line, base::LaunchOptions());
  int rv = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      test_child_process, TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

class CrashpadLinuxConfigTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::SetPosixFilePermissions(temp_dir_.GetPath(), 0755));
    config_path_ = temp_dir_.GetPath().Append(kCrashpadConfigFileName);
    uid_ = geteuid();
  }

  void WriteConfigFile(std::string_view content, int mode = 0644) {
    ASSERT_TRUE(base::WriteFile(config_path_, content));
    ASSERT_TRUE(base::SetPosixFilePermissions(config_path_, mode));
  }

  CrashpadLinux::Config ReadConfig() {
    return CrashpadLinux::ReadConfigFileForTesting(config_path_, uid_);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath config_path_;
  uid_t uid_ = 0;
};

TEST_F(CrashpadLinuxConfigTest, GetConfigFilePathReturnsDefaultPath) {
  EXPECT_EQ(CrashpadLinux::GetConfigFilePath(),
            base::FilePath("/etc/chrome-remote-desktop/crashpad.json"));
}

TEST_F(CrashpadLinuxConfigTest, AbsentFileReturnsDefaults) {
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, AbsentParentDirectoryReturnsDefaults) {
  CrashpadLinux::Config config = CrashpadLinux::ReadConfigFileForTesting(
      temp_dir_.GetPath().Append("missing").Append(kCrashpadConfigFileName),
      uid_);
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, ValidStagingUrlAndRateLimitOverrides) {
  ASSERT_NO_FATAL_FAILURE(WriteConfigFile(kValidOverrideConfigJson));
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_EQ(config.upload_url, "https://clients2.google.com/cr/staging_report");
  EXPECT_TRUE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, ExplicitDisableRateLimitFalse) {
  ASSERT_NO_FATAL_FAILURE(WriteConfigFile(
      R"({"upload_url": "https://clients2.google.com/cr/staging_report", )"
      R"("disable_rate_limit": false})"));
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_EQ(config.upload_url, "https://clients2.google.com/cr/staging_report");
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, ValidLoopbackHttpUrls) {
  for (const char* url :
       {"http://127.0.0.1:8080/cr/report", "http://localhost:9000/cr/report"}) {
    SCOPED_TRACE(url);
    base::DictValue dict;
    dict.Set(kCrashpadUploadUrlKey, url);
    std::string json;
    ASSERT_TRUE(base::JSONWriter::Write(dict, &json));
    ASSERT_NO_FATAL_FAILURE(WriteConfigFile(json));

    CrashpadLinux::Config config = ReadConfig();
    EXPECT_EQ(config.upload_url, url);
    EXPECT_FALSE(config.disable_rate_limit);
  }
}

TEST_F(CrashpadLinuxConfigTest, InvalidUploadUrlsIgnored) {
  // Full URL validation matrix is covered by
  // CrashpadDatabaseManagerTest.IsValidCrashpadUploadUrlOverride.
  for (const char* invalid_url :
       {"", "http://example.com/cr/report",
        "https://clients2.google.com/cr/report#frag"}) {
    SCOPED_TRACE(invalid_url);
    base::DictValue dict;
    dict.Set(kCrashpadUploadUrlKey, invalid_url);
    std::string json;
    ASSERT_TRUE(base::JSONWriter::Write(dict, &json));
    ASSERT_NO_FATAL_FAILURE(WriteConfigFile(json));

    CrashpadLinux::Config config = ReadConfig();
    EXPECT_FALSE(config.upload_url.has_value());
  }
}

TEST_F(CrashpadLinuxConfigTest, WrongValueTypesIgnored) {
  ASSERT_NO_FATAL_FAILURE(
      WriteConfigFile(R"({"upload_url": 123, "disable_rate_limit": "true"})"));
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, PartialValidFieldsAndUnknownKeys) {
  // Invalid upload_url string does not drop a valid disable_rate_limit.
  ASSERT_NO_FATAL_FAILURE(
      WriteConfigFile(R"({"upload_url": "http://example.com/cr/report", )"
                      R"("disable_rate_limit": true})"));
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_TRUE(config.disable_rate_limit);

  // Wrong-typed upload_url does not drop a valid disable_rate_limit.
  ASSERT_NO_FATAL_FAILURE(
      WriteConfigFile(R"({"upload_url": 123, "disable_rate_limit": true})"));
  config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_TRUE(config.disable_rate_limit);

  // Wrong-typed disable_rate_limit does not drop a valid upload_url.
  ASSERT_NO_FATAL_FAILURE(WriteConfigFile(
      R"({"upload_url": "https://clients2.google.com/cr/staging_report", )"
      R"("disable_rate_limit": "true"})"));
  config = ReadConfig();
  EXPECT_EQ(config.upload_url, "https://clients2.google.com/cr/staging_report");
  EXPECT_FALSE(config.disable_rate_limit);

  // Unrecognized keys are warned on without dropping valid overrides.
  ASSERT_NO_FATAL_FAILURE(WriteConfigFile(
      R"({"upload_url": "https://clients2.google.com/cr/staging_report", )"
      R"("disable_rate_limit": true, "UploadUrl": "ignored"})"));
  config = ReadConfig();
  EXPECT_EQ(config.upload_url, "https://clients2.google.com/cr/staging_report");
  EXPECT_TRUE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, GroupWritableFileRejected) {
  ASSERT_NO_FATAL_FAILURE(WriteConfigFile(kValidOverrideConfigJson, 0664));
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, WorldWritableFileRejected) {
  for (int mode : {0646, 0666}) {
    SCOPED_TRACE(mode);
    ASSERT_NO_FATAL_FAILURE(WriteConfigFile(kValidOverrideConfigJson, mode));
    CrashpadLinux::Config config = ReadConfig();
    EXPECT_FALSE(config.upload_url.has_value());
    EXPECT_FALSE(config.disable_rate_limit);
  }
}

TEST_F(CrashpadLinuxConfigTest, InsecureParentDirectoryRejected) {
  ASSERT_NO_FATAL_FAILURE(WriteConfigFile(kValidOverrideConfigJson, 0644));
  for (int dir_mode : {0775, 0757}) {
    SCOPED_TRACE(dir_mode);
    ASSERT_TRUE(base::SetPosixFilePermissions(temp_dir_.GetPath(), dir_mode));
    CrashpadLinux::Config config = ReadConfig();
    EXPECT_FALSE(config.upload_url.has_value());
    EXPECT_FALSE(config.disable_rate_limit);
  }
  ASSERT_TRUE(base::SetPosixFilePermissions(temp_dir_.GetPath(), 0755));
}

TEST_F(CrashpadLinuxConfigTest, SymlinkParentDirectoryRejected) {
  base::FilePath real_dir = temp_dir_.GetPath().Append("real");
  ASSERT_TRUE(base::CreateDirectory(real_dir));
  ASSERT_TRUE(base::SetPosixFilePermissions(real_dir, 0755));
  base::FilePath real_file = real_dir.Append(kCrashpadConfigFileName);
  ASSERT_TRUE(base::WriteFile(real_file, kValidOverrideConfigJson));
  ASSERT_TRUE(base::SetPosixFilePermissions(real_file, 0644));

  base::FilePath link_dir = temp_dir_.GetPath().Append("link_dir");
  ASSERT_TRUE(base::CreateSymbolicLink(real_dir, link_dir));

  CrashpadLinux::Config config = CrashpadLinux::ReadConfigFileForTesting(
      link_dir.Append(kCrashpadConfigFileName), uid_);
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, UnexpectedOwnerUidRejected) {
  // Note: this fails the parent-directory owner check in `ReadSecureFile()`,
  // since unprivileged tests cannot chown the file separately from its
  // directory.
  ASSERT_NO_FATAL_FAILURE(WriteConfigFile(kValidOverrideConfigJson));
  CrashpadLinux::Config config =
      CrashpadLinux::ReadConfigFileForTesting(config_path_, uid_ + 1);
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, SymlinkFileRejected) {
  base::FilePath target_path = temp_dir_.GetPath().Append("target.json");
  ASSERT_TRUE(base::WriteFile(target_path, kValidOverrideConfigJson));
  ASSERT_TRUE(base::SetPosixFilePermissions(target_path, 0644));
  ASSERT_TRUE(base::CreateSymbolicLink(target_path, config_path_));

  CrashpadLinux::Config config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, UnreadableFileRejected) {
  if (uid_ == 0) {
    GTEST_SKIP() << "Root bypasses mode 0000 read permission checks.";
  }
  ASSERT_NO_FATAL_FAILURE(WriteConfigFile(kValidOverrideConfigJson, 0000));
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, MalformedJsonRejected) {
  ASSERT_NO_FATAL_FAILURE(WriteConfigFile("{not-valid-json"));
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, NonDictJsonRootRejected) {
  for (std::string_view non_dict_json : {"[]", R"("string")", "123"}) {
    SCOPED_TRACE(non_dict_json);
    ASSERT_NO_FATAL_FAILURE(WriteConfigFile(non_dict_json));
    CrashpadLinux::Config config = ReadConfig();
    EXPECT_FALSE(config.upload_url.has_value());
    EXPECT_FALSE(config.disable_rate_limit);
  }
}

TEST_F(CrashpadLinuxConfigTest, AcceptsConfigAtExactMaxSize) {
  constexpr std::string_view kPrefix =
      R"({"upload_url": "https://clients2.google.com/cr/staging_report", )"
      R"("pad": ")";
  constexpr std::string_view kSuffix = R"("})";
  const size_t pad_bytes =
      kMaxCrashpadConfigBytes - kPrefix.size() - kSuffix.size();
  const std::string exact_max_json =
      std::string(kPrefix) + std::string(pad_bytes, 'a') + std::string(kSuffix);
  ASSERT_EQ(exact_max_json.size(), kMaxCrashpadConfigBytes);

  ASSERT_NO_FATAL_FAILURE(WriteConfigFile(exact_max_json));
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_EQ(config.upload_url, "https://clients2.google.com/cr/staging_report");
}

TEST_F(CrashpadLinuxConfigTest, RejectsOversizedConfig) {
  constexpr std::string_view kPrefix =
      R"({"upload_url": "https://clients2.google.com/cr/staging_report", )"
      R"("pad": ")";
  constexpr std::string_view kSuffix = R"("})";
  const size_t pad_bytes =
      kMaxCrashpadConfigBytes - kPrefix.size() - kSuffix.size() + 1;
  const std::string oversized_json =
      std::string(kPrefix) + std::string(pad_bytes, 'a') + std::string(kSuffix);
  ASSERT_EQ(oversized_json.size(), kMaxCrashpadConfigBytes + 1);

  ASSERT_NO_FATAL_FAILURE(WriteConfigFile(oversized_json));
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, DirectoryConfigPathRejected) {
  ASSERT_TRUE(base::CreateDirectory(config_path_));
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

TEST_F(CrashpadLinuxConfigTest, FifoConfigPathRejectedWithoutBlocking) {
  ASSERT_EQ(mkfifo(config_path_.value().c_str(), 0644), 0);
  CrashpadLinux::Config config = ReadConfig();
  EXPECT_FALSE(config.upload_url.has_value());
  EXPECT_FALSE(config.disable_rate_limit);
}

}  // namespace remoting
