// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/mac/sandbox_mac.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <fcntl.h>
#include <libproc.h>
#include <unistd.h>

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
#include "base/mac/mac_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/unguessable_token.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
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

class NetworkTestContentBrowserClient : public ContentBrowserClient {
 public:
  NetworkTestContentBrowserClient(base::FilePath data_dir,
                                  base::FilePath cache_dir)
      : data_dir_(std::move(data_dir)), cache_dir_(std::move(cache_dir)) {}

  std::vector<base::FilePath> GetNetworkContextsParentDirectory() override {
    return {data_dir_, cache_dir_};
  }

  std::optional<base::FilePath> GetNetworkContextsDataDirectory() override {
    return data_dir_;
  }

  std::optional<base::FilePath> GetNetworkContextsCacheDirectory() override {
    return cache_dir_;
  }

 private:
  base::FilePath data_dir_;
  base::FilePath cache_dir_;
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

MULTIPROCESS_TEST_MAIN(MetalShaderCacheRestrictedProcess) {
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
  test_denied_file(temp_dir.Append(unauth_file));
  test_denied_file(user_dir.Append(unauth_file));

  return 0;
}

class SandboxMacMetalShaderCacheTest
    : public SandboxMacTest,
      public testing::WithParamInterface<sandbox::mojom::Sandbox> {};

// Verifies that the GPU and on-device model execution sandboxes restrict
// Darwin user directory access to only known Metal shader cache files and
// directories in the Darwin user cache dir:
// - Allows creating allowed directories (com.apple.metalfe,
//   com.apple.gpuarchiver, com.apple.metal, and subdirectories).
// - Allows creating, writing, and reading allowed cache files
//   (modules.timestamp, functions.data/list, and libraries.data/list).
// - Denies creating unauthorized directories or files in the cache directory.
// - Denies creating unauthorized files in the Darwin temp or user directories.
TEST_P(SandboxMacMetalShaderCacheTest, DarwinUserDirsRestricted) {
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
      "MetalShaderCacheRestrictedProcess", GetParam(),
      /*append_temp_dir_suffix=*/false,
      base::BindOnce(
          [](const std::string& bundle_id,
             sandbox::SandboxSerializer* serializer) {
            CHECK(serializer->SetParameter(
                sandbox::policy::kParamHelperBundleId, bundle_id));
          },
          extra_data_));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SandboxMacMetalShaderCacheTest,
    testing::Values(sandbox::mojom::Sandbox::kGpu,
                    sandbox::mojom::Sandbox::kOnDeviceModelExecution));

MULTIPROCESS_TEST_MAIN(RendererPidinfoRestrictedProcess) {
  CheckCreateSeatbeltServer();

  // Ensure the renderer can query `proc_pidinfo` for itself (`(target self)`).
  proc_bsdinfo bsd_info;
  CHECK_EQ(
      proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info)),
      static_cast<int>(sizeof(bsd_info)));

  // Ensure the renderer is denied when querying `proc_pidinfo` for another
  // process (here, the parent process).
  CHECK_EQ(
      proc_pidinfo(getppid(), PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info)),
      0);
  CHECK_EQ(errno, EPERM);

  return 0;
}

// Verifies that, when `kMacSandboxAllowRendererPidinfo` is disabled, the
// renderer sandbox allows `proc_pidinfo` on the current process (`getpid()`)
// while denying `proc_pidinfo` on other processes (`getppid()`) with `EPERM`.
TEST_F(SandboxMacTest, RendererPidinfoRestricted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kMacSandboxAllowRendererPidinfo);

  ExecuteWithParams("RendererPidinfoRestrictedProcess",
                    sandbox::mojom::Sandbox::kRenderer);
}

MULTIPROCESS_TEST_MAIN(RendererPidinfoUnrestrictedProcess) {
  CheckCreateSeatbeltServer();

  // Ensure the renderer can query `proc_pidinfo` for both itself and another
  // process (the parent process) when unrestricted `process-info-pidinfo` is
  // enabled.
  proc_bsdinfo bsd_info;
  CHECK_EQ(
      proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info)),
      static_cast<int>(sizeof(bsd_info)));
  CHECK_EQ(
      proc_pidinfo(getppid(), PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info)),
      static_cast<int>(sizeof(bsd_info)));

  return 0;
}

// Verifies that, when `kMacSandboxAllowRendererPidinfo` is enabled, the
// renderer sandbox allows `proc_pidinfo` on both the current process
// (`getpid()`) and other processes (`getppid()`).
TEST_F(SandboxMacTest, RendererPidinfoUnrestricted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMacSandboxAllowRendererPidinfo);

  ExecuteWithParams("RendererPidinfoUnrestrictedProcess",
                    sandbox::mojom::Sandbox::kRenderer);
}

constexpr base::FilePath::CharType kTestStoragePartitionRelPath[] =
    FILE_PATH_LITERAL("Default/Storage/ext/test_ext/def");
constexpr base::FilePath::CharType kTestNestedStoragePartitionRelPath[] =
    FILE_PATH_LITERAL("Default/Storage/ext/test_iwa_part1/test_iwa_part2/def");

MULTIPROCESS_TEST_MAIN(NetworkDataMigrationRestrictedProcess) {
  std::string extra_data = GetExtraDataValue();
  CHECK(!extra_data.empty());
  std::vector<std::string> parts = base::SplitString(
      extra_data, ";", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  CHECK_EQ(parts.size(), 2u);
  base::FilePath user_data_dir(parts[0]);
  base::FilePath user_cache_dir(parts[1]);

  CheckCreateSeatbeltServer();

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
    CHECK_EQ(base::as_string_view(buffer), kData);
  };

  auto test_denied_file = [](const base::FilePath& path) {
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    CHECK(!file.IsValid());
    CHECK_EQ(file.error_details(), base::File::FILE_ERROR_ACCESS_DENIED);
  };

  // Allowed directories and files.
  base::FilePath network_dir =
      user_data_dir.Append("Default").Append("Network");
  CHECK(base::CreateDirectory(network_dir));
  test_allowed_file(network_dir.Append("cookies.db"));

  base::FilePath safe_browsing_dir =
      user_data_dir.Append("Profile 1").Append("Safe Browsing Network");
  CHECK(base::CreateDirectory(safe_browsing_dir));
  test_allowed_file(safe_browsing_dir.Append("safebrowsing.db"));

  base::FilePath shared_dict_dir =
      user_data_dir.Append("Default").Append("Shared Dictionary");
  CHECK(base::CreateDirectory(shared_dict_dir));
  test_allowed_file(shared_dict_dir.Append("db"));

  base::FilePath cache_dir = user_cache_dir.Append("Default").Append("Cache");
  CHECK(base::CreateDirectory(cache_dir));
  test_allowed_file(cache_dir.Append("cache_file"));

  base::FilePath root_cache_dir = user_cache_dir.Append("Cache");
  CHECK(base::CreateDirectory(root_cache_dir));
  test_allowed_file(root_cache_dir.Append("root_cache_file"));

  // Storage partition cache subdirectory.
  base::FilePath partition_cache_parent =
      user_cache_dir.Append(kTestStoragePartitionRelPath);
  base::FilePath partition_cache_dir = partition_cache_parent.Append("Cache");
  CHECK(base::CreateDirectory(partition_cache_dir));
  test_allowed_file(partition_cache_dir.Append("cache_file"));

  // Storage partition data subdirectories in user-data-dir.
  base::FilePath partition_data_parent =
      user_data_dir.Append(kTestStoragePartitionRelPath);
  base::FilePath partition_network_dir =
      partition_data_parent.Append("Network");
  CHECK(base::CreateDirectory(partition_network_dir));
  test_allowed_file(partition_network_dir.Append("cookies.db"));

  base::FilePath partition_shared_dict_dir =
      partition_data_parent.Append("Shared Dictionary");
  CHECK(base::CreateDirectory(partition_shared_dict_dir));
  test_allowed_file(partition_shared_dict_dir.Append("db"));

  // Nested storage partition (simulating multi-level IWA domains) cache
  // subdirectory.
  base::FilePath nested_partition_cache_parent =
      user_cache_dir.Append(kTestNestedStoragePartitionRelPath);
  base::FilePath nested_partition_cache_dir =
      nested_partition_cache_parent.Append("Cache");
  CHECK(base::CreateDirectory(nested_partition_cache_dir));
  test_allowed_file(nested_partition_cache_dir.Append("cache_file"));

  // Nested storage partition data subdirectories in user-data-dir.
  base::FilePath nested_partition_data_parent =
      user_data_dir.Append(kTestNestedStoragePartitionRelPath);
  base::FilePath nested_partition_network_dir =
      nested_partition_data_parent.Append("Network");
  CHECK(base::CreateDirectory(nested_partition_network_dir));
  test_allowed_file(nested_partition_network_dir.Append("cookies.db"));

  base::FilePath nested_partition_shared_dict_dir =
      nested_partition_data_parent.Append("Shared Dictionary");
  CHECK(base::CreateDirectory(nested_partition_shared_dict_dir));
  test_allowed_file(nested_partition_shared_dict_dir.Append("db"));

  // Denied directories and files.
  base::File::Error dir_error = base::File::FILE_OK;
  CHECK(!base::CreateDirectoryAndGetError(
      user_data_dir.Append("Default").Append("UnauthorizedDir"), &dir_error));
  CHECK_EQ(dir_error, base::File::FILE_ERROR_ACCESS_DENIED);

  CHECK(!base::CreateDirectoryAndGetError(
      partition_cache_parent.Append("Code Cache"), &dir_error));
  CHECK_EQ(dir_error, base::File::FILE_ERROR_ACCESS_DENIED);
  CHECK(!base::CreateDirectoryAndGetError(
      partition_data_parent.Append("Code Cache"), &dir_error));
  CHECK_EQ(dir_error, base::File::FILE_ERROR_ACCESS_DENIED);
  CHECK(!base::CreateDirectoryAndGetError(
      partition_data_parent.Append("Preferences"), &dir_error));
  CHECK_EQ(dir_error, base::File::FILE_ERROR_ACCESS_DENIED);

  CHECK(!base::CreateDirectoryAndGetError(
      nested_partition_cache_parent.Append("Code Cache"), &dir_error));
  CHECK_EQ(dir_error, base::File::FILE_ERROR_ACCESS_DENIED);
  CHECK(!base::CreateDirectoryAndGetError(
      nested_partition_data_parent.Append("Code Cache"), &dir_error));
  CHECK_EQ(dir_error, base::File::FILE_ERROR_ACCESS_DENIED);
  CHECK(!base::CreateDirectoryAndGetError(
      nested_partition_data_parent.Append("Preferences"), &dir_error));
  CHECK_EQ(dir_error, base::File::FILE_ERROR_ACCESS_DENIED);

  test_denied_file(user_data_dir.Append("Default").Append("Preferences"));
  test_denied_file(user_data_dir.Append("Default").Append("NetworkExtra"));
  test_denied_file(user_data_dir.Append("unauthorized.txt"));
  test_denied_file(partition_data_parent.Append("unauthorized.txt"));
  test_denied_file(partition_data_parent.Append("NetworkExtra"));
  test_denied_file(nested_partition_data_parent.Append("unauthorized.txt"));
  test_denied_file(nested_partition_data_parent.Append("NetworkExtra"));
  test_denied_file(user_cache_dir.Append("unauthorized.txt"));
  test_denied_file(user_cache_dir.Append("Default").Append("unauthorized.txt"));
  test_denied_file(user_cache_dir.Append("Default").Append("CacheExtra"));
  test_denied_file(partition_cache_parent.Append("unauthorized.txt"));
  test_denied_file(nested_partition_cache_parent.Append("unauthorized.txt"));

  // Symbolic link creation is denied even in writable directories.
  CHECK(!base::CreateSymbolicLink(network_dir.Append("cookies.db"),
                                  network_dir.Append("symlink")));
  CHECK_EQ(base::File::GetLastFileError(),
           base::File::FILE_ERROR_ACCESS_DENIED);
  CHECK(!base::CreateSymbolicLink(cache_dir.Append("cache_file"),
                                  cache_dir.Append("symlink")));
  CHECK_EQ(base::File::GetLastFileError(),
           base::File::FILE_ERROR_ACCESS_DENIED);

  return 0;
}

// Verifies that when `kTriggerNetworkDataMigration` is enabled, the macOS
// network service sandbox restricts file access in `USER_DATA_DIR` to only
// the designated Network, Safe Browsing Network, and Shared Dictionary
// subdirectories (and `USER_CACHE_DIR`), denying access to other profile paths.
TEST_F(SandboxMacTest, NetworkDataMigrationRestricted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kTriggerNetworkDataMigration},
      /*disabled_features=*/{kMacSandboxAllowNetworkSymlinkCreation});

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath user_data_dir = temp_dir.GetPath().Append("UserData");
  base::FilePath user_cache_dir = temp_dir.GetPath().Append("Cache");
  ASSERT_TRUE(base::CreateDirectory(user_data_dir));
  ASSERT_TRUE(base::CreateDirectory(user_data_dir.Append("Default")));
  ASSERT_TRUE(base::CreateDirectory(user_data_dir.Append("Profile 1")));
  ASSERT_TRUE(base::CreateDirectory(
      user_data_dir.Append(kTestStoragePartitionRelPath)));
  ASSERT_TRUE(base::CreateDirectory(
      user_data_dir.Append(kTestNestedStoragePartitionRelPath)));
  ASSERT_TRUE(base::CreateDirectory(user_cache_dir));
  ASSERT_TRUE(base::CreateDirectory(user_cache_dir.Append("Default")));
  ASSERT_TRUE(base::CreateDirectory(
      user_cache_dir.Append(kTestStoragePartitionRelPath)));
  ASSERT_TRUE(base::CreateDirectory(
      user_cache_dir.Append(kTestNestedStoragePartitionRelPath)));

  user_data_dir = sandbox::policy::GetCanonicalPath(user_data_dir);
  user_cache_dir = sandbox::policy::GetCanonicalPath(user_cache_dir);

  extra_data_ =
      base::StrCat({user_data_dir.value(), ";", user_cache_dir.value()});

  NetworkTestContentBrowserClient test_client(user_data_dir, user_cache_dir);
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);
  base::ScopedClosureRunner restore_client(base::BindOnce(
      [](ContentBrowserClient* client) { SetBrowserClientForTesting(client); },
      old_client));

  ExecuteWithParams("NetworkDataMigrationRestrictedProcess",
                    sandbox::mojom::Sandbox::kNetwork,
                    /*append_temp_dir_suffix=*/false);
}

MULTIPROCESS_TEST_MAIN(NetworkDataMigrationUnrestrictedProcess) {
  std::string extra_data = GetExtraDataValue();
  CHECK(!extra_data.empty());
  std::vector<std::string> parts = base::SplitString(
      extra_data, ";", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  CHECK_EQ(parts.size(), 2u);
  base::FilePath user_data_dir(parts[0]);
  base::FilePath user_cache_dir(parts[1]);

  CheckCreateSeatbeltServer();

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
    CHECK_EQ(base::as_string_view(buffer), kData);
  };

  // When migration is disabled, broad access to the entire storage path is
  // permitted.
  base::FilePath unauth_dir =
      user_data_dir.Append("Default").Append("UnauthorizedDir");
  CHECK(base::CreateDirectory(unauth_dir));
  test_allowed_file(user_data_dir.Append("Default").Append("Preferences"));
  test_allowed_file(user_data_dir.Append("unauthorized.txt"));
  test_allowed_file(user_cache_dir.Append("cache_file"));

  // Symbolic link creation is denied even in writable directories.
  CHECK(!base::CreateSymbolicLink(user_data_dir.Append("unauthorized.txt"),
                                  user_data_dir.Append("symlink")));
  CHECK_EQ(base::File::GetLastFileError(),
           base::File::FILE_ERROR_ACCESS_DENIED);
  CHECK(!base::CreateSymbolicLink(user_cache_dir.Append("cache_file"),
                                  user_cache_dir.Append("symlink")));
  CHECK_EQ(base::File::GetLastFileError(),
           base::File::FILE_ERROR_ACCESS_DENIED);

  return 0;
}

// Verifies that when `kTriggerNetworkDataMigration` is disabled, the macOS
// network service sandbox falls back to the legacy behavior of granting
// recursive subpath access across the entire storage directory.
TEST_F(SandboxMacTest, NetworkDataMigrationUnrestricted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kTriggerNetworkDataMigration,
                             kMacSandboxAllowNetworkSymlinkCreation});

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath user_data_dir = temp_dir.GetPath().Append("UserData");
  base::FilePath user_cache_dir = temp_dir.GetPath().Append("Cache");
  ASSERT_TRUE(base::CreateDirectory(user_data_dir));
  ASSERT_TRUE(base::CreateDirectory(user_data_dir.Append("Default")));
  ASSERT_TRUE(base::CreateDirectory(user_cache_dir));

  user_data_dir = sandbox::policy::GetCanonicalPath(user_data_dir);
  user_cache_dir = sandbox::policy::GetCanonicalPath(user_cache_dir);

  extra_data_ =
      base::StrCat({user_data_dir.value(), ";", user_cache_dir.value()});

  NetworkTestContentBrowserClient test_client(user_data_dir, user_cache_dir);
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);
  base::ScopedClosureRunner restore_client(base::BindOnce(
      [](ContentBrowserClient* client) { SetBrowserClientForTesting(client); },
      old_client));

  ExecuteWithParams("NetworkDataMigrationUnrestrictedProcess",
                    sandbox::mojom::Sandbox::kNetwork,
                    /*append_temp_dir_suffix=*/false);
}

MULTIPROCESS_TEST_MAIN(NetworkSymlinkAllowedProcess) {
  std::string extra_data = GetExtraDataValue();
  CHECK(!extra_data.empty());
  std::vector<std::string> parts = base::SplitString(
      extra_data, ";", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  CHECK_EQ(parts.size(), 2u);
  base::FilePath user_data_dir(parts[0]);
  base::FilePath user_cache_dir(parts[1]);

  CheckCreateSeatbeltServer();

  base::FilePath network_dir =
      user_data_dir.Append("Default").Append("Network");
  CHECK(base::CreateDirectory(network_dir));

  base::FilePath cookies_file = network_dir.Append("cookies.db");
  base::File file(cookies_file,
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  CHECK(file.IsValid());
  file.Close();

  base::FilePath cache_dir = user_cache_dir.Append("Cache");
  CHECK(base::CreateDirectory(cache_dir));
  base::FilePath cache_file = cache_dir.Append("cache_file");
  base::File file2(cache_file,
                   base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  CHECK(file2.IsValid());
  file2.Close();

  // Symbolic link creation is allowed when
  // `kMacSandboxAllowNetworkSymlinkCreation` is enabled.
  CHECK(base::CreateSymbolicLink(cookies_file, network_dir.Append("symlink")));
  CHECK(base::IsLink(network_dir.Append("symlink")));
  CHECK(base::CreateSymbolicLink(cache_file, cache_dir.Append("symlink")));
  CHECK(base::IsLink(cache_dir.Append("symlink")));

  return 0;
}

// Verifies that when `kMacSandboxAllowNetworkSymlinkCreation` is enabled, the
// macOS network service sandbox allows creating symbolic links within writable
// network service directories.
TEST_F(SandboxMacTest, NetworkSymlinkAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMacSandboxAllowNetworkSymlinkCreation);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath user_data_dir = temp_dir.GetPath().Append("UserData");
  base::FilePath user_cache_dir = temp_dir.GetPath().Append("Cache");
  ASSERT_TRUE(base::CreateDirectory(user_data_dir));
  ASSERT_TRUE(base::CreateDirectory(user_data_dir.Append("Default")));
  ASSERT_TRUE(base::CreateDirectory(user_cache_dir));

  user_data_dir = sandbox::policy::GetCanonicalPath(user_data_dir);
  user_cache_dir = sandbox::policy::GetCanonicalPath(user_cache_dir);

  extra_data_ =
      base::StrCat({user_data_dir.value(), ";", user_cache_dir.value()});

  NetworkTestContentBrowserClient test_client(user_data_dir, user_cache_dir);
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);
  base::ScopedClosureRunner restore_client(base::BindOnce(
      [](ContentBrowserClient* client) { SetBrowserClientForTesting(client); },
      old_client));

  ExecuteWithParams("NetworkSymlinkAllowedProcess",
                    sandbox::mojom::Sandbox::kNetwork,
                    /*append_temp_dir_suffix=*/false);
}

MULTIPROCESS_TEST_MAIN(WebNNModelCompilationProcess) {
  std::string extra_data = GetExtraDataValue();
  CHECK(!extra_data.empty());
  std::vector<std::string> parts = base::SplitString(
      extra_data, ";", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  CHECK_EQ(parts.size(), 2u);
  base::FilePath temp_dir(parts[0]);
  std::string token = parts[1];

  CheckCreateSeatbeltServer();

  // The compiler process can create and manage its own staging directories
  // under `TemporaryItems` and `webnn_compiler_protected`.
  base::FilePath temp_items_dir = temp_dir.Append("TemporaryItems");
  CHECK(base::CreateDirectory(temp_items_dir));
  base::FilePath staged_model_dir =
      temp_items_dir.Append(base::StrCat({"staged_", token, ".mlmodelc"}));
  CHECK(base::CreateDirectory(staged_model_dir));
  base::File staged_file(
      staged_model_dir.Append("model.mil"),
      base::File::FLAG_CREATE | base::File::FLAG_WRITE | base::File::FLAG_READ);
  CHECK(staged_file.IsValid());
  constexpr std::string_view kData = "model";
  CHECK(staged_file.WriteAndCheck(0, base::as_byte_span(kData)));
  staged_file.Close();

  base::FilePath compiler_protected_dir =
      temp_dir.Append("webnn_compiler_protected");
  CHECK(base::CreateDirectory(compiler_protected_dir));
  base::FilePath compiler_dir =
      compiler_protected_dir.Append(base::StrCat({"compiler_", token}));
  CHECK(base::Move(staged_model_dir, compiler_dir));
  CHECK(base::DeletePathRecursively(compiler_dir));

  // Unlinking or renaming the `webnn_compiler_protected` directory itself is
  // denied.
  CHECK_EQ(rmdir(compiler_protected_dir.value().c_str()), -1);
  CHECK_EQ(errno, EPERM);
  CHECK_EQ(
      rename(compiler_protected_dir.value().c_str(),
             temp_dir.Append("renamed_compiler_protected").value().c_str()),
      -1);
  CHECK_EQ(errno, EPERM);

  // Creating symbolic links is denied anywhere in `darwin-user-temp-dir`.
  std::string symlink_name = base::StrCat({"symlink_", token});
  CHECK(!base::CreateSymbolicLink(temp_items_dir,
                                  compiler_protected_dir.Append(symlink_name)));
  CHECK_EQ(base::File::GetLastFileError(),
           base::File::FILE_ERROR_ACCESS_DENIED);
  CHECK(!base::CreateSymbolicLink(temp_items_dir,
                                  temp_items_dir.Append(symlink_name)));
  CHECK_EQ(base::File::GetLastFileError(),
           base::File::FILE_ERROR_ACCESS_DENIED);

  // Creating, reading, writing, or unlinking anything under
  // `webnn_gpu_protected` is denied.
  base::FilePath gpu_protected_dir = temp_dir.Append("webnn_gpu_protected");
  base::File::Error dir_error = base::File::FILE_OK;
  CHECK(!base::CreateDirectoryAndGetError(
      gpu_protected_dir.Append(base::StrCat({"sub_", token})), &dir_error));
  CHECK_EQ(dir_error, base::File::FILE_ERROR_ACCESS_DENIED);

  base::File denied_create(
      gpu_protected_dir.Append(base::StrCat({"new_file_", token})),
      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  CHECK(!denied_create.IsValid());
  CHECK_EQ(denied_create.error_details(), base::File::FILE_ERROR_ACCESS_DENIED);

  base::FilePath existing_file =
      gpu_protected_dir.Append(base::StrCat({"existing_file_", token}));
  base::File denied_read(existing_file,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  CHECK(!denied_read.IsValid());
  CHECK_EQ(denied_read.error_details(), base::File::FILE_ERROR_ACCESS_DENIED);

  CHECK_EQ(unlink(existing_file.value().c_str()), -1);
  CHECK_EQ(errno, EPERM);

  CHECK_EQ(rename(existing_file.value().c_str(),
                  temp_dir.Append(base::StrCat({"stolen_file_", token}))
                      .value()
                      .c_str()),
           -1);
  CHECK_EQ(errno, EPERM);

  return 0;
}

TEST_F(SandboxMacTest, WebNNModelCompilationGpuProtectedRestricted) {
  char dir_path[PATH_MAX + 1];
  ASSERT_NE(confstr(_CS_DARWIN_USER_TEMP_DIR, dir_path, sizeof(dir_path)), 0u);

  base::FilePath canonical_temp_dir =
      sandbox::policy::GetCanonicalPath(base::FilePath(dir_path));
  base::FilePath gpu_protected_dir =
      canonical_temp_dir.Append("webnn_gpu_protected");
  ASSERT_TRUE(base::CreateDirectory(gpu_protected_dir));

  std::string token = base::UnguessableToken::Create().ToString();
  base::FilePath existing_file =
      gpu_protected_dir.Append(base::StrCat({"existing_file_", token}));
  ASSERT_TRUE(base::WriteFile(existing_file, "gpu"));
  base::ScopedClosureRunner cleanup(
      base::BindOnce([](const base::FilePath& file) { base::DeleteFile(file); },
                     existing_file));

  extra_data_ = base::StrCat({canonical_temp_dir.value(), ";", token});

  ExecuteWithParams("WebNNModelCompilationProcess",
                    sandbox::mojom::Sandbox::kWebNNModelCompilation,
                    /*append_temp_dir_suffix=*/false);
}

}  // namespace content
