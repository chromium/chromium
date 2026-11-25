// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_linux.h"

#include <dlfcn.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/run_until.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "remoting/base/crash/crash_reporting_crashpad.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/zlib/google/compression_utils.h"

namespace remoting {
namespace {

constexpr char kTestXdgRuntimeDirSwitch[] = "test-crashpad-xdg-runtime-dir";
constexpr char kTestConfigPathSwitch[] = "test-crashpad-config-path";
constexpr char kTestExpectedUploadUrlSwitch[] =
    "test-crashpad-expected-upload-url";
constexpr char kTestHandlerPidSwitch[] = "test-crashpad-handler-pid";
constexpr int kInheritedCrashpadSocketFd = 42;
constexpr int kInheritedShutdownPipeFd = 43;

#if !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER)
bool IsLibcurlAvailable() {
  for (const char* soname : {"libcurl.so", "libcurl-gnutls.so.4",
                             "libcurl-nss.so.4", "libcurl.so.4"}) {
    void* handle = dlopen(soname, RTLD_LAZY | RTLD_LOCAL);
    if (handle) {
      dlclose(handle);
      return true;
    }
  }
  return false;
}

bool IsYamaPtraceRestricted() {
  std::string contents;
  if (!base::ReadFileToString(
          base::FilePath("/proc/sys/kernel/yama/ptrace_scope"), &contents)) {
    return false;
  }
  int scope = 0;
  return base::StringToInt(base::TrimWhitespaceASCII(contents, base::TRIM_ALL),
                           &scope) &&
         scope >= 2;
}
#endif

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

MULTIPROCESS_TEST_MAIN(EndToEndCrashingClientChild) {
  // Suppress core file generation for this intentional SIGSEGV child.
  struct rlimit no_core = {0, 0};
  setrlimit(RLIMIT_CORE, &no_core);

  int handler_pid_int = 0;
  if (!base::StringToInt(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              kTestHandlerPidSwitch),
          &handler_pid_int)) {
    return 1;
  }

  // Reset SIGSEGV to default before Crashpad installs its handler so Crashpad
  // chains to SIG_DFL rather than base::debug::StackDumpSignalHandler,
  // avoiding symbolized stack-dump log noise and dependence on base's handler.
  signal(SIGSEGV, SIG_DFL);

  if (!InitializeCrashpadClient(base::ScopedFD(kInheritedCrashpadSocketFd),
                                static_cast<pid_t>(handler_pid_int))) {
    return 2;
  }

  raise(SIGSEGV);
  return 0;
}

MULTIPROCESS_TEST_MAIN(EndToEndSupervisorChild) {
  base::ScopedFD shutdown_pipe_read_fd(kInheritedShutdownPipeFd);

  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  const base::FilePath xdg_runtime_dir =
      cmd->GetSwitchValuePath(kTestXdgRuntimeDirSwitch);
  const base::FilePath config_path =
      cmd->GetSwitchValuePath(kTestConfigPathSwitch);
  const std::string expected_upload_url =
      cmd->GetSwitchValueASCII(kTestExpectedUploadUrlSwitch);
  if (xdg_runtime_dir.empty() || config_path.empty() ||
      expected_upload_url.empty()) {
    return 1;
  }

  setenv("XDG_RUNTIME_DIR", xdg_runtime_dir.value().c_str(), /*overwrite=*/1);
  setenv("NO_PROXY", "127.0.0.1,localhost", /*overwrite=*/1);
  setenv("no_proxy", "127.0.0.1,localhost", /*overwrite=*/1);

  CrashpadLinux::SetConfigPathForTesting(config_path);
  // Verify the config override took effect before starting the handler so a
  // regression in ReadConfig() never uploads a test minidump to production.
  if (CrashpadLinux::ReadConfig().upload_url != expected_upload_url) {
    return 5;
  }

  InitializeCrashpadReporting();

  base::ScopedFD handler_sock;
  pid_t handler_pid = -1;
  if (!GetCrashpadHandlerSocket(handler_sock, handler_pid) ||
      !handler_sock.is_valid()) {
    return 2;
  }

  base::CommandLine client_cmd(base::GetMultiProcessTestChildBaseCommandLine());
  client_cmd.RemoveSwitch(switches::kTestChildProcess);
  client_cmd.RemoveSwitch(kTestXdgRuntimeDirSwitch);
  client_cmd.RemoveSwitch(kTestConfigPathSwitch);
  client_cmd.RemoveSwitch(kTestExpectedUploadUrlSwitch);
  client_cmd.AppendSwitchASCII(kTestHandlerPidSwitch,
                               base::NumberToString(handler_pid));
  base::LaunchOptions client_options;
  client_options.fds_to_remap.emplace_back(handler_sock.get(),
                                           kInheritedCrashpadSocketFd);
  base::Process crashing_child = base::SpawnMultiProcessTestChild(
      "EndToEndCrashingClientChild", client_cmd, client_options);
  handler_sock.reset();
  if (!crashing_child.IsValid()) {
    return 6;
  }

  int client_exit_code = 0;
  // Wait action_timeout() so a hung crashing child causes the supervisor to
  // return exit code 3 before the parent's 2 * action_timeout() upload wait
  // expires.
  if (!base::WaitForMultiprocessTestChildExit(
          crashing_child, TestTimeouts::action_timeout(), &client_exit_code)) {
    base::TerminateMultiProcessTestChild(crashing_child, 0, /*wait=*/true);
    return 3;
  }
  // Process::WaitForExitWithTimeout reports -1 when terminated by a signal.
  if (client_exit_code != -1) {
    return 4;
  }

  // Block until the parent closes the write end of the shutdown pipe (which
  // also delivers EOF automatically if the parent terminates unexpectedly).
  char byte = 0;
  HANDLE_EINTR(read(shutdown_pipe_read_fd.get(), &byte, 1));
  return 0;
}

TEST(CrashpadLinuxTest, EndToEndClientCrashAndUploadViaConfigOverride) {
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
  GTEST_SKIP() << "Intentional SIGSEGV crash tests are not supported under "
                  "sanitizers.";
#else
  static constexpr char kMockCrashReportId[] = "mock_linux_crash_report_id_42";

  if (getuid() == 0) {
    GTEST_SKIP() << "Test requires a non-root UID so XDG_RUNTIME_DIR isolates "
                    "the Crashpad database.";
  }
  if (!IsLibcurlAvailable()) {
    GTEST_SKIP() << "Crashpad HTTP transport on Linux requires libcurl.";
  }
  if (IsYamaPtraceRestricted()) {
    GTEST_SKIP()
        << "Crashpad ptrace attachment requires kernel.yama.ptrace_scope < 2.";
  }

  base::test::SingleThreadTaskEnvironment task_environment;

  base::FilePath exe_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_dir));
  ASSERT_TRUE(base::PathExists(exe_dir.Append("crashpad-handler")) ||
              base::PathExists(exe_dir.Append("remoting_crashpad_handler")));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath xdg_runtime_dir = temp_dir.GetPath().Append("runtime");
  ASSERT_TRUE(base::CreateDirectory(xdg_runtime_dir));
  ASSERT_EQ(chmod(xdg_runtime_dir.value().c_str(), 0700), 0);

  std::atomic<bool> upload_captured{false};
  base::test::TestFuture<net::test_server::HttpRequest> upload_future;

  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindRepeating(
      [](std::atomic<bool>* captured,
         base::RepeatingCallback<void(net::test_server::HttpRequest)> on_upload,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (!base::StartsWith(request.relative_url, "/cr/report",
                              base::CompareCase::SENSITIVE)) {
          return nullptr;
        }
        if (!captured->exchange(true)) {
          on_upload.Run(request);
        }
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/plain");
        response->set_content(kMockCrashReportId);
        return response;
      },
      &upload_captured, upload_future.GetSequenceBoundRepeatingCallback()));
  ASSERT_TRUE(test_server.Start());

  const std::string upload_url = test_server.GetURL("/cr/report").spec();
  const base::FilePath config_path =
      temp_dir.GetPath().Append(kCrashpadConfigFileName);
  base::DictValue config_dict;
  config_dict.Set(kCrashpadUploadUrlKey, upload_url);
  std::string config_json;
  ASSERT_TRUE(base::JSONWriter::Write(config_dict, &config_json));
  ASSERT_TRUE(base::WriteFile(config_path, config_json));
  ASSERT_EQ(chmod(config_path.value().c_str(), 0644), 0);

  int shutdown_pipe[2];
  ASSERT_EQ(pipe(shutdown_pipe), 0);
  base::ScopedFD shutdown_read_fd(shutdown_pipe[0]);
  base::ScopedFD shutdown_write_fd(shutdown_pipe[1]);

  base::CommandLine supervisor_cmd(
      base::GetMultiProcessTestChildBaseCommandLine());
  supervisor_cmd.AppendSwitchPath(kTestXdgRuntimeDirSwitch, xdg_runtime_dir);
  supervisor_cmd.AppendSwitchPath(kTestConfigPathSwitch, config_path);
  supervisor_cmd.AppendSwitchASCII(kTestExpectedUploadUrlSwitch, upload_url);

  base::LaunchOptions supervisor_options;
  supervisor_options.fds_to_remap.emplace_back(shutdown_read_fd.get(),
                                               kInheritedShutdownPipeFd);
  base::Process supervisor_process = base::SpawnMultiProcessTestChild(
      "EndToEndSupervisorChild", supervisor_cmd, supervisor_options);
  shutdown_read_fd.reset();
  ASSERT_TRUE(supervisor_process.IsValid());
  const base::TimeDelta cleanup_stage_timeout =
      TestTimeouts::action_timeout() / 2;
  base::ScopedClosureRunner supervisor_cleanup(base::BindOnce(
      [](base::ScopedFD* write_fd, base::Process* proc,
         base::TimeDelta timeout) {
        write_fd->reset();
        int cleanup_exit_code = 0;
        if (!base::WaitForMultiprocessTestChildExit(*proc, timeout,
                                                    &cleanup_exit_code)) {
          LOG(ERROR) << "Timed out waiting for supervisor child; terminating.";
          base::TerminateMultiProcessTestChild(*proc, 0, /*wait=*/true);
        } else if (cleanup_exit_code != 0) {
          LOG(ERROR) << "Supervisor child exited with code "
                     << cleanup_exit_code;
        }
      },
      &shutdown_write_fd, &supervisor_process, cleanup_stage_timeout));

  // Periodically wake the task environment while RunUntil() polls external
  // child process exit and external Crashpad database file updates.
  base::RepeatingTimer poll_timer;
  poll_timer.Start(FROM_HERE, base::Milliseconds(25), base::DoNothing());

  std::optional<int> early_supervisor_exit_code;
  auto has_supervisor_exited_early = [&]() {
    if (early_supervisor_exit_code.has_value()) {
      return true;
    }
    int code = 0;
    if (base::WaitForMultiprocessTestChildExit(supervisor_process,
                                               base::TimeDelta(), &code)) {
      early_supervisor_exit_code = code;
      // POSIX waitpid() has now reaped supervisor_process; disarm cleanup so it
      // does not wait/signal a reaped PID.
      shutdown_write_fd.reset();
      supervisor_cleanup.ReplaceClosure(base::DoNothing());
      return true;
    }
    return false;
  };

  {
    // Total worst-case wait budget across all four stages is
    // 3.5 * action_timeout() (35 s at default 10 s, under the 45 s launcher
    // timeout): 2 * action_timeout() here for 3-process startup, minidump
    // capture, and libcurl upload, plus 0.5 * action_timeout() each for the
    // database completion poll, supervisor exit, and temp_dir deletion below.
    base::test::ScopedRunLoopTimeout upload_timeout(
        FROM_HERE, 2 * TestTimeouts::action_timeout());
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return upload_future.IsReady() || has_supervisor_exited_early();
    }));
  }
  ASSERT_FALSE(early_supervisor_exit_code.has_value())
      << "Supervisor child exited early with code "
      << *early_supervisor_exit_code;

  const net::test_server::HttpRequest& request = upload_future.Get();
  EXPECT_EQ(request.method, net::test_server::METHOD_POST);
  EXPECT_NE(request.relative_url.find("product=Chromoting_Linux"),
            std::string::npos)
      << request.relative_url;
  auto ua_it = request.headers.find("User-Agent");
  ASSERT_NE(ua_it, request.headers.end());
  EXPECT_TRUE(base::StartsWith(ua_it->second, "Crashpad/",
                               base::CompareCase::SENSITIVE))
      << ua_it->second;

  std::string body = request.content;
  auto encoding_it = request.headers.find("Content-Encoding");
  if (encoding_it != request.headers.end() && encoding_it->second == "gzip") {
    ASSERT_TRUE(compression::GzipUncompress(request.content, &body));
  }
  EXPECT_NE(body.find("Chromoting_Linux"), std::string::npos);
  EXPECT_NE(body.find("MDMP"), std::string::npos);

  const base::FilePath expected_db_path =
      xdg_runtime_dir.Append("crd_crashpad");
  auto db = crashpad::CrashReportDatabase::InitializeWithoutCreating(
      expected_db_path);
  ASSERT_NE(db, nullptr);

  std::vector<crashpad::CrashReportDatabase::Report> completed_reports;
  {
    base::test::ScopedRunLoopTimeout db_timeout(FROM_HERE,
                                                cleanup_stage_timeout);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      if (has_supervisor_exited_early()) {
        return true;
      }
      completed_reports.clear();
      return db->GetCompletedReports(&completed_reports) ==
                 crashpad::CrashReportDatabase::kNoError &&
             !completed_reports.empty() && completed_reports[0].uploaded;
    }));
  }
  ASSERT_FALSE(early_supervisor_exit_code.has_value())
      << "Supervisor child exited early with code "
      << *early_supervisor_exit_code;

  ASSERT_EQ(completed_reports.size(), 1u);
  EXPECT_TRUE(completed_reports[0].uploaded);
  EXPECT_EQ(completed_reports[0].id, kMockCrashReportId);
  db.reset();

  shutdown_write_fd.reset();
  int supervisor_exit_code = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      supervisor_process, cleanup_stage_timeout, &supervisor_exit_code));
  supervisor_cleanup.ReplaceClosure(base::DoNothing());
  EXPECT_EQ(supervisor_exit_code, 0);

  // The double-forked Crashpad handler exits asynchronously after its last
  // client socket closes; retry deleting temp_dir briefly.
  {
    base::test::ScopedRunLoopTimeout temp_dir_timeout(FROM_HERE,
                                                      cleanup_stage_timeout);
    EXPECT_TRUE(base::test::RunUntil([&]() { return temp_dir.Delete(); }));
  }
  poll_timer.Stop();
#endif
}

}  // namespace remoting
