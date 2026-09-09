// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/mac/sandbox_mac.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <dirent.h>
#include <fcntl.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/mac/mac_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_timeouts.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "sandbox/mac/sandbox_serializer.h"
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/boringssl/src/include/openssl/rand.h"
#import "ui/base/clipboard/clipboard_util_mac.h"

namespace content {
namespace {

// crbug.com/740009: This allows the unit test to cleanup temporary directories,
// and is safe since this is only a unit test.
constexpr char kTempDirSuffix[] =
    "(allow file* (subpath \"/private/var/folders\"))";
constexpr char kExtraDataArg[] = "extra-data";

class SandboxMacTest : public base::MultiProcessTest {
 protected:
  base::CommandLine MakeCmdLine(const std::string& procname) override {
    base::CommandLine cl = MultiProcessTest::MakeCmdLine(procname);
    cl.AppendArg(
        base::StringPrintf("%s%d", sandbox::switches::kSeatbeltClient, pipe_));
    if (!extra_data_.empty()) {
      cl.AppendSwitchASCII(kExtraDataArg, extra_data_);
    }
    return cl;
  }

  void ExecuteWithParams(const std::string& procname,
                         sandbox::mojom::Sandbox sandbox_type,
                         const std::string& suffix = "") {
    std::string profile =
        sandbox::policy::GetSandboxProfile(sandbox_type) + kTempDirSuffix;
    sandbox::SandboxSerializer serializer(
        sandbox::SandboxSerializer::Target::kSource);

    serializer.SetProfile(profile);
    base::EnvironmentMap env;
    if (!suffix.empty()) {
      env[base::env_vars::kDirHelperUserDirSuffix] = suffix;
    }
    SetupSandboxParameters(sandbox_type,
                           *base::CommandLine::ForCurrentProcess(), env,
                           &serializer);
    std::string error, serialized;
    CHECK(serializer.SerializePolicy(serialized, error)) << error;

    sandbox::SeatbeltExecClient client;
    pipe_ = client.GetReadFD();
    ASSERT_GE(pipe_, 0);

    base::LaunchOptions options;
    options.fds_to_remap.emplace_back(pipe_, pipe_);
    if (!suffix.empty()) {
      options.environment[base::env_vars::kDirHelperUserDirSuffix] = suffix;
    }

    base::Process process = SpawnChildWithOptions(procname, options);
    ASSERT_TRUE(process.IsValid());
    ASSERT_TRUE(client.SendPolicy(serialized));

    int rv = -1;
    ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
        process, TestTimeouts::action_timeout(), &rv));
    EXPECT_EQ(0, rv);
  }

  void ExecuteInAllSandboxTypes(const std::string& multiprocess_main,
                                base::RepeatingClosure after_each) {
    constexpr sandbox::mojom::Sandbox kSandboxTypes[] = {
        sandbox::mojom::Sandbox::kAudio,
        sandbox::mojom::Sandbox::kCdm,
        sandbox::mojom::Sandbox::kGpu,
        sandbox::mojom::Sandbox::kPrintBackend,
        sandbox::mojom::Sandbox::kPrintCompositor,
        sandbox::mojom::Sandbox::kRenderer,
        sandbox::mojom::Sandbox::kService,
        sandbox::mojom::Sandbox::kServiceWithJit,
        sandbox::mojom::Sandbox::kUtility,
    };

    for (const auto type : kSandboxTypes) {
      ExecuteWithParams(multiprocess_main, type);
      if (!after_each.is_null()) {
        after_each.Run();
      }
    }
  }

  int pipe_{0};
  std::string extra_data_{};
};

void CheckCreateSeatbeltServer() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& argv = cl->argv();
  std::vector<char*> argv_cstr(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
    argv_cstr[i] = const_cast<char*>(argv[i].c_str());
  }
  auto result = sandbox::SeatbeltExecServer::CreateFromArguments(
      argv_cstr[0], argv_cstr.size(), argv_cstr.data());

  CHECK(result.sandbox_required);
  CHECK(result.server);
  CHECK(result.server->InitializeSandbox());
}

std::string GetExtraDataValue() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  return cl->GetSwitchValueASCII(kExtraDataArg);
}

}  // namespace

MULTIPROCESS_TEST_MAIN(RendererWriteProcess) {
  CheckCreateSeatbeltServer();

  // Test that the renderer cannot write to the home directory.
  NSString* test_file = [NSHomeDirectory()
      stringByAppendingPathComponent:@"e539dd6f-6b38-4f6a-af2c-809a5ea96e1c"];
  int fd = HANDLE_EINTR(
      open(base::SysNSStringToUTF8(test_file).c_str(), O_CREAT | O_RDWR));
  CHECK(-1 == fd);
  CHECK_EQ(errno, EPERM);

  return 0;
}

TEST_F(SandboxMacTest, RendererCannotWriteHomeDir) {
  ExecuteWithParams("RendererWriteProcess", sandbox::mojom::Sandbox::kRenderer);
}

MULTIPROCESS_TEST_MAIN(ClipboardAccessProcess) {
  CheckCreateSeatbeltServer();

  std::string pasteboard_name = GetExtraDataValue();
  CHECK(!pasteboard_name.empty());
  CHECK([NSPasteboard pasteboardWithName:base::SysUTF8ToNSString(
                                             pasteboard_name)] == nil);
  CHECK(NSPasteboard.generalPasteboard == nil);

  return 0;
}

TEST_F(SandboxMacTest, ClipboardAccess) {
  scoped_refptr<ui::UniquePasteboard> pb = new ui::UniquePasteboard;
  ASSERT_TRUE(pb->get());
  EXPECT_EQ(pb->get().types.count, 0U);

  extra_data_ = base::SysNSStringToUTF8(pb->get().name);

  ExecuteInAllSandboxTypes("ClipboardAccessProcess",
                           base::BindRepeating(
                               [](scoped_refptr<ui::UniquePasteboard> pb) {
                                 ASSERT_EQ([[pb->get() types] count], 0U);
                               },
                               pb));
}

MULTIPROCESS_TEST_MAIN(SSLProcess) {
  CheckCreateSeatbeltServer();

  // Ensure that RAND_bytes is functional within the sandbox.
  uint8_t byte;
  CHECK(RAND_bytes(&byte, 1) == 1);
  return 0;
}

TEST_F(SandboxMacTest, SSLInitTest) {
  ExecuteInAllSandboxTypes("SSLProcess", base::RepeatingClosure());
}

// This test checks to make sure that `__builtin_available()` (and therefore the
// Objective-C equivalent `@available()`) work within a sandbox. When revving
// the macOS releases supported by Chromium, bump this up. This value
// specifically matches the oldest macOS release supported by Chromium.
MULTIPROCESS_TEST_MAIN(BuiltinAvailable) {
  CheckCreateSeatbeltServer();

  if (!__builtin_available(macOS 13, *)) {
    return 15;
  }

  return 0;
}

TEST_F(SandboxMacTest, BuiltinAvailable) {
  ExecuteInAllSandboxTypes("BuiltinAvailable", {});
}

MULTIPROCESS_TEST_MAIN(NetworkProcessPrefs) {
  CheckCreateSeatbeltServer();

  const std::string kBundleId(base::apple::BaseBundleID());
  const std::string kUserName = base::SysNSStringToUTF8(NSUserName());
  const std::vector<std::string> kPaths = {
      "/Library/Managed Preferences/.GlobalPreferences.plist",
      base::StrCat({"/Library/Managed Preferences/", kBundleId, ".plist"}),
      base::StrCat({"/Library/Managed Preferences/", kUserName,
                    "/.GlobalPreferences.plist"}),
      base::StrCat({"/Library/Managed Preferences/", kUserName, "/", kBundleId,
                    ".plist"}),
      base::StrCat({"/Library/Preferences/", kBundleId, ".plist"}),
      base::StrCat({"/Users/", kUserName,
                    "/Library/Preferences/com.apple.security.plist"}),
      base::StrCat(
          {"/Users/", kUserName, "/Library/Preferences/", kBundleId, ".plist"}),
  };

  for (const auto& path : kPaths) {
    // Use open rather than stat to test file-read-data rules.
    base::ScopedFD fd(open(path.c_str(), O_RDONLY));
    PCHECK(fd.is_valid() || errno == ENOENT) << path;
  }

  return 0;
}

TEST_F(SandboxMacTest, NetworkProcessPrefs) {
  ExecuteWithParams("NetworkProcessPrefs", sandbox::mojom::Sandbox::kNetwork);
}

MULTIPROCESS_TEST_MAIN(ProxyResolverProcess) {
  CheckCreateSeatbeltServer();
  return 0;
}

// Verifies the kProxyResolver seatbelt profile initializes successfully with
// the required parameters supplied by SetupSandboxParameters().
TEST_F(SandboxMacTest, ProxyResolverInitializesSandbox) {
  ExecuteWithParams("ProxyResolverProcess",
                    sandbox::mojom::Sandbox::kProxyResolver);
}

MULTIPROCESS_TEST_MAIN(GpuIsolatedDarwinUserDirsProcess) {
  CheckCreateSeatbeltServer();

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::optional<std::string> suffix =
      env->GetVar(base::env_vars::kDirHelperUserDirSuffix);
  CHECK(suffix.has_value() && !suffix->empty());

  std::string parent_file_name =
      base::StringPrintf("parent_file_%s.txt", suffix->c_str());
  std::string new_file_name =
      base::StringPrintf("new_test_file_%s.txt", suffix->c_str());

  auto CheckWriteDenied = [&](const base::FilePath& parent_dir) {
    // Check creating a new file is denied.
    base::FilePath new_file_path = parent_dir.Append(new_file_name);
    base::File new_file(
        new_file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    CHECK(!new_file.IsValid());
    CHECK_EQ(new_file.error_details(), base::File::FILE_ERROR_ACCESS_DENIED);

    // Check modifying an existing file is denied.
    base::FilePath existing_file_path = parent_dir.Append(parent_file_name);
    base::File existing_file(existing_file_path,
                             base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    CHECK(!existing_file.IsValid());
    CHECK_EQ(existing_file.error_details(),
             base::File::FILE_ERROR_ACCESS_DENIED);
  };

  auto CheckWriteAllowed = [](const base::FilePath& child_dir) {
    // Check creating a new file is allowed.
    base::FilePath new_file_path = child_dir.Append("new_test_file.txt");
    base::File new_file(
        new_file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    CHECK(new_file.IsValid());

    // Check modifying an existing file is allowed.
    base::FilePath existing_file_path = child_dir.Append("child_file.txt");
    base::File existing_file(existing_file_path,
                             base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    CHECK(existing_file.IsValid());
  };

  auto CheckReadDenied = [&](const base::FilePath& parent_dir) {
    // Check directory read (opendir) is denied.
    DIR* dir = opendir(parent_dir.value().c_str());
    CHECK(!dir);
    CHECK(errno == EACCES || errno == EPERM);

    // Check file read is denied.
    base::FilePath file_path = parent_dir.Append(parent_file_name);
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(!file.IsValid());
    CHECK_EQ(file.error_details(), base::File::FILE_ERROR_ACCESS_DENIED);
  };

  auto CheckReadAllowed = [](const base::FilePath& child_dir) {
    // Check directory read (opendir) is allowed.
    DIR* dir = opendir(child_dir.value().c_str());
    CHECK(dir);
    closedir(dir);

    // Check file read is allowed.
    base::FilePath file_path = child_dir.Append("child_file.txt");
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(file.IsValid());
    char buf[100];
    std::optional<size_t> bytes_read =
        file.ReadAtCurrentPos(base::as_writable_bytes(base::span(buf)));
    CHECK(bytes_read.has_value());
    CHECK_GT(*bytes_read, 0u);
    std::string contents(buf, *bytes_read);
    CHECK_EQ(contents, "sandbox_read_test");
  };

  base::DarwinUserDirectory dirs[] = {
      base::DarwinUserDirectory::kUser,
      base::DarwinUserDirectory::kUserCache,
      base::DarwinUserDirectory::kUserTemp,
  };

  for (auto type : dirs) {
    base::FilePath child = base::GetDarwinUserDirectory(type);
    base::FilePath parent = child.DirName();

    CheckWriteDenied(parent);
    CheckReadDenied(parent);

    CheckWriteAllowed(child);
    CheckReadAllowed(child);
  }

  return 0;
}

// Verifies that when sandbox-child-user-dir, sandbox-child-user-cache-dir,
// and sandbox-child-user-temp-dir are set, the GPU process seatbelt sandbox
// denies access to the parent directories and allows access to the isolated
// child directories.
TEST_F(SandboxMacTest, GpuIsolatedDarwinUserDirs) {
  base::FilePath system_user =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUser);
  base::FilePath system_cache =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUserCache);
  base::FilePath system_temp =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUserTemp);

  ASSERT_FALSE(system_user.empty());
  ASSERT_FALSE(system_cache.empty());
  ASSERT_FALSE(system_temp.empty());

  // Add a random suffix to avoid bot test flakiness
  std::string suffix = base::Uuid::GenerateRandomV4().AsLowercaseString();

  base::FilePath child_user_raw = system_user.Append(suffix);
  base::FilePath child_cache_raw = system_cache.Append(suffix);
  base::FilePath child_temp_raw = system_temp.Append(suffix);

  ASSERT_TRUE(base::CreateDirectory(child_user_raw));
  ASSERT_TRUE(base::CreateDirectory(child_cache_raw));
  ASSERT_TRUE(base::CreateDirectory(child_temp_raw));

  base::FilePath child_user = sandbox::policy::GetCanonicalPath(child_user_raw);
  base::FilePath child_cache =
      sandbox::policy::GetCanonicalPath(child_cache_raw);
  base::FilePath child_temp = sandbox::policy::GetCanonicalPath(child_temp_raw);

  ASSERT_FALSE(child_user.empty());
  ASSERT_FALSE(child_cache.empty());
  ASSERT_FALSE(child_temp.empty());

  std::string parent_file_name =
      base::StringPrintf("parent_file_%s.txt", suffix.c_str());
  base::FilePath parent_user_file = system_user.Append(parent_file_name);
  base::FilePath parent_cache_file = system_cache.Append(parent_file_name);
  base::FilePath parent_temp_file = system_temp.Append(parent_file_name);

  const std::string kTestData = "sandbox_read_test";
  ASSERT_TRUE(base::WriteFile(parent_user_file, kTestData));
  ASSERT_TRUE(base::WriteFile(parent_cache_file, kTestData));
  ASSERT_TRUE(base::WriteFile(parent_temp_file, kTestData));

  base::FilePath child_user_file = child_user_raw.Append("child_file.txt");
  base::FilePath child_cache_file = child_cache_raw.Append("child_file.txt");
  base::FilePath child_temp_file = child_temp_raw.Append("child_file.txt");

  ASSERT_TRUE(base::WriteFile(child_user_file, kTestData));
  ASSERT_TRUE(base::WriteFile(child_cache_file, kTestData));
  ASSERT_TRUE(base::WriteFile(child_temp_file, kTestData));

  // Ensure they are cleaned up recursively when the test finishes.
  base::ScopedClosureRunner cleanup(base::BindOnce(
      [](base::FilePath child_user, base::FilePath child_cache,
         base::FilePath child_temp, base::FilePath parent_user_file,
         base::FilePath parent_cache_file, base::FilePath parent_temp_file) {
        base::DeletePathRecursively(child_user);
        base::DeletePathRecursively(child_cache);
        base::DeletePathRecursively(child_temp);
        base::DeleteFile(parent_user_file);
        base::DeleteFile(parent_cache_file);
        base::DeleteFile(parent_temp_file);
      },
      child_user, child_cache, child_temp, parent_user_file, parent_cache_file,
      parent_temp_file));

  ExecuteWithParams("GpuIsolatedDarwinUserDirsProcess",
                    sandbox::mojom::Sandbox::kGpu, suffix);
}

}  // namespace content
