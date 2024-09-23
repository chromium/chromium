// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <IOSurface/IOSurface.h>

#include <ifaddrs.h>
#include <servers/bootstrap.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <string_view>

#include "base/apple/bundle_locations.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/mac_util.h"
#include "base/process/kill.h"
#include "base/system/sys_info.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "content/test/test_content_client.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "sandbox/policy/mac/common.sb.h"
#include "sandbox/policy/mac/params.h"
#include "sandbox/policy/mac/renderer.sb.h"
#include "sandbox/policy/mac/sandbox_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace content {

namespace {

void SetParametersForTest(sandbox::SandboxCompiler* compiler,
                          const base::FilePath& logging_path,
                          const base::FilePath& executable_path,
                          bool use_syscall_filter) {
  bool enable_logging = true;
  CHECK(compiler->SetBooleanParameter(sandbox::policy::kParamEnableLogging,
                                      enable_logging));
  CHECK(compiler->SetBooleanParameter(
      sandbox::policy::kParamDisableSandboxDenialLogging, !enable_logging));

  std::string homedir =
      sandbox::policy::GetCanonicalPath(base::GetHomeDir()).value();
  CHECK(
      compiler->SetParameter(sandbox::policy::kParamHomedirAsLiteral, homedir));

  int32_t major_version, minor_version, bugfix_version;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  int32_t os_version = (major_version * 100) + minor_version;
  CHECK(compiler->SetParameter(sandbox::policy::kParamOsVersion,
                               base::NumberToString(os_version)));

  std::string bundle_path =
      sandbox::policy::GetCanonicalPath(base::apple::MainBundlePath()).value();
  CHECK(compiler->SetParameter(sandbox::policy::kParamBundlePath, bundle_path));

  CHECK(compiler->SetParameter(sandbox::policy::kParamBundleId,
                               "com.google.Chrome.test.sandbox"));
  CHECK(compiler->SetParameter(sandbox::policy::kParamBrowserPid,
                               base::NumberToString(getpid())));

  CHECK(compiler->SetParameter(sandbox::policy::kParamLogFilePath,
                               logging_path.value()));

  CHECK(compiler->SetParameter(sandbox::policy::kParamExecutablePath,
                               executable_path.value()));

  CHECK(compiler->SetBooleanParameter(sandbox::policy::kParamFilterSyscalls,
                                      use_syscall_filter));
}

}  // namespace

// These tests check that the V2 sandbox compiles, initializes, and
// correctly enforces resource access on all macOS versions. Note that
// with the exception of certain controlled locations, such as a dummy
// log file, these tests cannot check that write access to system files
// is blocked. These tests run on developers' machines and bots, so
// if the write access goes through, that machine could be corrupted.
class SandboxV2Test : public base::MultiProcessTest {};

MULTIPROCESS_TEST_MAIN(SandboxProfileProcess) {
  TestContentClient content_client;
  const std::string profile =
      std::string(sandbox::policy::kSeatbeltPolicyString_common) +
      sandbox::policy::kSeatbeltPolicyString_renderer;
  sandbox::SandboxCompiler compiler;
  compiler.SetProfile(profile);

  // Create the logging file and pass /bin/ls as the executable path.
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  CHECK(temp_dir.IsValid());
  base::FilePath temp_path = temp_dir.GetPath();
  temp_path = sandbox::policy::GetCanonicalPath(temp_path);
  const base::FilePath log_file = temp_path.Append("log-file");
  const base::FilePath exec_file("/bin/ls");

  // TODO(crbug.com/40273168): re-enable syscall filter for this test.
  // SandboxV2Test.SandboxProfileTest uses system() which uses a denied syscall,
  // which should cause the test to fail.
  SetParametersForTest(&compiler, log_file, exec_file,
                       /*use_syscall_filter=*/false);

  std::string error;
  bool result = compiler.CompileAndApplyProfile(error);
  CHECK(result) << error;

  // Test the properties of the sandbox profile.
  constexpr std::string_view log_msg = "logged";
  CHECK(base::WriteFile(log_file, std::string_view(log_msg)));
  // Log file is write only.
  char read_buf[log_msg.size()];
  CHECK_EQ(-1, base::ReadFile(log_file, read_buf, sizeof(read_buf)));

  // Try executing the blessed binary.
  CHECK_NE(-1, system(exec_file.value().c_str()));

  // Try and realpath a file.
  char resolved_name[4096];
  CHECK_NE(nullptr, realpath(log_file.value().c_str(), resolved_name));

  // Test shared memory access.
  int shm_fd = shm_open("apple.shm.notification_center", O_RDONLY, 0644);
  CHECK_GE(shm_fd, 0);

  // Test mach service access. The port is leaked because the multiprocess
  // test exits quickly after this look up.
  mach_port_t service_port;
  kern_return_t status = bootstrap_look_up(
      bootstrap_port, "com.apple.system.logger", &service_port);
  CHECK_EQ(status, BOOTSTRAP_SUCCESS) << bootstrap_strerror(status);

  mach_port_t forbidden_mach;
  status = bootstrap_look_up(bootstrap_port, "com.apple.cfprefsd.daemon",
                             &forbidden_mach);
  CHECK_NE(BOOTSTRAP_SUCCESS, status);

  // Read bundle contents.
  base::FilePath bundle_path = base::apple::MainBundlePath();
  struct stat st;
  CHECK_NE(-1, stat(bundle_path.value().c_str(), &st));

  // Test that general file system access isn't available.
  base::FilePath ascii_path("/usr/share/misc/ascii");
  std::string ascii_contents;
  CHECK(!base::ReadFileToStringWithMaxSize(ascii_path, &ascii_contents, 4096));

  base::FilePath system_certs(
      "/System/Library/Keychains/SystemRootCertificates.keychain");
  std::string keychain_contents;
  CHECK(!base::ReadFileToStringWithMaxSize(system_certs, &keychain_contents,
                                           4096));

  // Check that not all sysctls, including those that can get the MAC address,
  // are allowed. See crbug.com/738129.
  struct ifaddrs* ifap;
  CHECK_EQ(-1, getifaddrs(&ifap));

  std::vector<uint8_t> sysctl_data(4096);
  size_t data_size = sysctl_data.size();
  CHECK_EQ(0,
           sysctlbyname("hw.ncpu", sysctl_data.data(), &data_size, nullptr, 0));

  CHECK(!base::Process::Current().CreationTime().is_null());

  return 0;
}

TEST_F(SandboxV2Test, SandboxProfileTest) {
  base::Process process = SpawnChild("SandboxProfileProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

}  // namespace content
