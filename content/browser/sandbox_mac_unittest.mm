// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/mac/sandbox_mac.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <fcntl.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/mac/mac_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/unguessable_token.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "sandbox/mac/sandbox_serializer.h"
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "sandbox/policy/mac/params.h"
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
                         bool append_temp_dir_suffix = true,
                         base::OnceCallback<void(sandbox::SandboxSerializer*)>
                             configure_serializer = {}) {
    std::string profile = sandbox::policy::GetSandboxProfile(sandbox_type);
    if (append_temp_dir_suffix) {
      profile += kTempDirSuffix;
    }
    sandbox::SandboxSerializer serializer(
        sandbox::SandboxSerializer::Target::kSource);

    serializer.SetProfile(profile);
    SetupSandboxParameters(
        sandbox_type, *base::CommandLine::ForCurrentProcess(), &serializer);
    if (configure_serializer) {
      std::move(configure_serializer).Run(&serializer);
    }
    std::string error, serialized;
    CHECK(serializer.SerializePolicy(serialized, error)) << error;

    sandbox::SeatbeltExecClient client;
    pipe_ = client.GetReadFD();
    ASSERT_GE(pipe_, 0);

    base::LaunchOptions options;
    options.fds_to_remap.emplace_back(pipe_, pipe_);

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

constexpr char kTestHelperBundleIdPrefix[] = "org.chromium.test.gpu_cache.";

MULTIPROCESS_TEST_MAIN(GpuMetalCacheRestrictedProcess) {
  std::string helper_bundle_id = GetExtraDataValue();
  CHECK(!helper_bundle_id.empty());

  base::FilePath cache_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUserCache);
  base::FilePath temp_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUserTemp);
  base::FilePath user_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUser);
  CHECK(!cache_dir.empty());
  CHECK(!temp_dir.empty());
  CHECK(!user_dir.empty());

  CheckCreateSeatbeltServer();

  base::FilePath helper_dir = cache_dir.Append(helper_bundle_id);

  // Ensure we can create known Metal directories.
  base::FilePath metalfe_dir = helper_dir.Append("com.apple.metalfe");
  CHECK(base::CreateDirectory(metalfe_dir));

  base::FilePath gpuarchiver_dir = helper_dir.Append("com.apple.gpuarchiver");
  CHECK(base::CreateDirectory(gpuarchiver_dir));

  base::FilePath metal_dir = helper_dir.Append("com.apple.metal");
  CHECK(base::CreateDirectory(metal_dir));

  base::FilePath metal_arch1_dir = metal_dir.Append("16777235_355");
  CHECK(base::CreateDirectory(metal_arch1_dir));

  base::FilePath metal_arch2_dir = metal_dir.Append("32024");
  CHECK(base::CreateDirectory(metal_arch2_dir));

  base::FilePath metal_arch3_dir = metal_dir.Append("0123456789abcdef");
  CHECK(base::CreateDirectory(metal_arch3_dir));

  // Ensure we can create, write, and read known Metal cache files.
  auto test_allowed_file = [](const base::FilePath& path) {
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                              base::File::FLAG_READ);
    CHECK(file.IsValid());
    constexpr std::string_view kData = "test";
    CHECK(file.WriteAndCheck(0, base::as_byte_span(kData)));
    file.Close();

    base::File read_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(read_file.IsValid());
    std::array<char, kData.size()> buffer;
    CHECK(read_file.ReadAndCheck(0, base::as_writable_byte_span(buffer)));
    CHECK_EQ(std::string_view(buffer.data(), buffer.size()), kData);
  };

  test_allowed_file(metalfe_dir.Append("modules.timestamp"));
  test_allowed_file(metal_arch1_dir.Append("functions.data"));
  test_allowed_file(metal_arch1_dir.Append("functions.list"));
  test_allowed_file(metal_arch1_dir.Append("functions1.list"));
  test_allowed_file(metal_arch2_dir.Append("libraries.data"));
  test_allowed_file(metal_arch2_dir.Append("libraries.list"));

  // Ensure we deny directory creation outside of the allowed helper dir path.
  base::File::Error dir_error = base::File::FILE_OK;
  CHECK(!base::CreateDirectoryAndGetError(helper_dir.Append("unauthorized_dir"),
                                          &dir_error));
  CHECK_EQ(dir_error, base::File::FILE_ERROR_ACCESS_DENIED);

  // Ensure that we can't create files with unrecognized names in the metal
  // cache directories.
  auto test_denied_file = [](const base::FilePath& path) {
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    CHECK(!file.IsValid());
    CHECK_EQ(file.error_details(), base::File::FILE_ERROR_ACCESS_DENIED);
  };

  test_denied_file(helper_dir.Append("unauthorized.txt"));
  test_denied_file(metalfe_dir.Append("unauthorized.txt"));

  // Ensure that we can't create files in the base cache_dir, temp_dir, and
  // user_dir.
  std::string unauth_file =
      base::StrCat({"test_unauth_file_", helper_bundle_id});
  test_denied_file(cache_dir.Append(unauth_file));
  // TODO: uncomment the following line once WebNN model compilation has been
  // moved to a separate sandbox (https://crbug.com/524263705).
  // test_denied_file(temp_dir.Append(unauth_file));
  test_denied_file(user_dir.Append(unauth_file));

  return 0;
}

MULTIPROCESS_TEST_MAIN(GpuDarwinUserDirsUnrestrictedProcess) {
  std::string helper_bundle_id = GetExtraDataValue();
  CHECK(!helper_bundle_id.empty());

  base::FilePath cache_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUserCache);
  base::FilePath temp_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUserTemp);
  base::FilePath user_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUser);
  CHECK(!cache_dir.empty());
  CHECK(!temp_dir.empty());
  CHECK(!user_dir.empty());

  CheckCreateSeatbeltServer();

  base::FilePath helper_dir = cache_dir.Append(helper_bundle_id);

  // Ensure arbitrary file creation in Darwin cache, temp, and user dirs
  // succeeds without the sandbox restriction
  base::FilePath cache_file = helper_dir.Append("unauthorized.txt");
  base::File cache_base_file(cache_file,
                             base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  CHECK(cache_base_file.IsValid());

  base::FilePath temp_file =
      temp_dir.Append(base::StrCat({"test_unauth_temp_", helper_bundle_id}));
  base::File temp_base_file(temp_file,
                            base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  CHECK(temp_base_file.IsValid());

  base::FilePath user_file =
      user_dir.Append(base::StrCat({"test_unauth_user_", helper_bundle_id}));
  base::File user_base_file(user_file,
                            base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  CHECK(user_base_file.IsValid());

  return 0;
}

// Verifies that, when `kMacSandboxRestrictGpuDarwinUserDirs` is enabled, the
// GPU process sandbox restricts Darwin user directory access to only known
// Metal shader cache files and directories in the Darwin user cache dir:
// - Allows creating allowed directories (com.apple.metalfe,
//   com.apple.gpuarchiver, com.apple.metal, and subdirectories).
// - Allows creating, writing, and reading allowed cache files
//   (modules.timestamp, functions.data/list, and libraries.data/list).
// - Denies creating unauthorized directories or files in the cache directory.
// - Denies creating unauthorized files in the Darwin temp or user directories.
TEST_F(SandboxMacTest, GpuDarwinUserDirsRestricted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMacSandboxRestrictGpuDarwinUserDirs);

  // Provide a ~random bundle ID to avoid collisions when running tests in
  // parallel.
  extra_data_ = base::StrCat(
      {kTestHelperBundleIdPrefix, base::UnguessableToken::Create().ToString()});

  base::FilePath cache_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUserCache);
  CHECK(!cache_dir.empty());

  base::FilePath helper_dir = cache_dir.Append(extra_data_);
  base::DeletePathRecursively(helper_dir);
  ASSERT_TRUE(base::CreateDirectory(helper_dir));
  base::ScopedClosureRunner cleanup(base::BindOnce(
      [](const base::FilePath& path) { base::DeletePathRecursively(path); },
      helper_dir));

  ExecuteWithParams(
      "GpuMetalCacheRestrictedProcess", sandbox::mojom::Sandbox::kGpu,
      /*append_temp_dir_suffix=*/false,
      base::BindOnce(
          [](const std::string& bundle_id,
             sandbox::SandboxSerializer* serializer) {
            CHECK(serializer->SetParameter(
                sandbox::policy::kParamHelperBundleId, bundle_id));
          },
          extra_data_));
}

// Verifies that, when `kMacSandboxRestrictGpuDarwinUserDirs` is disabled, the
// GPU process sandbox falls back to the broad Darwin user directory rules,
// allowing arbitrary file creation in the three Darwin user directories.
TEST_F(SandboxMacTest, GpuDarwinUserDirsUnrestricted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kMacSandboxRestrictGpuDarwinUserDirs);

  // Provide a ~random bundle ID to avoid collisions when running tests in
  // parallel.
  extra_data_ = base::StrCat(
      {kTestHelperBundleIdPrefix, base::UnguessableToken::Create().ToString()});

  base::FilePath cache_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUserCache);
  base::FilePath temp_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUserTemp);
  base::FilePath user_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUser);
  CHECK(!cache_dir.empty());
  CHECK(!temp_dir.empty());
  CHECK(!user_dir.empty());

  base::FilePath helper_dir = cache_dir.Append(extra_data_);
  base::DeletePathRecursively(helper_dir);
  ASSERT_TRUE(base::CreateDirectory(helper_dir));
  base::ScopedClosureRunner cleanup(base::BindOnce(
      [](const base::FilePath& path) { base::DeletePathRecursively(path); },
      helper_dir));

  base::FilePath temp_file =
      temp_dir.Append(base::StrCat({"test_unauth_temp_", extra_data_}));
  base::ScopedClosureRunner cleanup_temp(base::BindOnce(
      [](const base::FilePath& path) { base::DeleteFile(path); }, temp_file));

  base::FilePath user_file =
      user_dir.Append(base::StrCat({"test_unauth_user_", extra_data_}));
  base::ScopedClosureRunner cleanup_user(base::BindOnce(
      [](const base::FilePath& path) { base::DeleteFile(path); }, user_file));

  ExecuteWithParams(
      "GpuDarwinUserDirsUnrestrictedProcess", sandbox::mojom::Sandbox::kGpu,
      /*append_temp_dir_suffix=*/false,
      base::BindOnce(
          [](const std::string& bundle_id,
             sandbox::SandboxSerializer* serializer) {
            CHECK(serializer->SetParameter(
                sandbox::policy::kParamHelperBundleId, bundle_id));
          },
          extra_data_));
}

}  // namespace content
