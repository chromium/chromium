// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <fcntl.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
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
#include "base/test/test_timeouts.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "sandbox/policy/mac/sandbox_mac.h"
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
                         sandbox::mojom::Sandbox sandbox_type) {
    std::string profile =
        sandbox::policy::GetSandboxProfile(sandbox_type) + kTempDirSuffix;
    sandbox::SandboxCompiler compiler;
    compiler.SetProfile(profile);
    SetupSandboxParameters(sandbox_type,
                           *base::CommandLine::ForCurrentProcess(), &compiler);
    sandbox::mac::SandboxPolicy policy;
    std::string error;
    ASSERT_TRUE(compiler.CompilePolicyToProto(policy, error)) << error;

    sandbox::SeatbeltExecClient client;
    pipe_ = client.GetReadFD();
    ASSERT_GE(pipe_, 0);

    base::LaunchOptions options;
    options.fds_to_remap.emplace_back(pipe_, pipe_);

    base::Process process = SpawnChildWithOptions(procname, options);
    ASSERT_TRUE(process.IsValid());
    ASSERT_TRUE(client.SendPolicy(policy));

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

  if (__builtin_available(macOS 11, *)) {
    // Can't negate a __builtin_available condition. But success!
  } else {
    return 15;
  }

  return 0;
}

TEST_F(SandboxMacTest, BuiltinAvailable) {
  ExecuteInAllSandboxTypes("BuiltinAvailable", {});
}

MULTIPROCESS_TEST_MAIN(NetworkProcessPrefs) {
  CheckCreateSeatbeltServer();

  const std::string kBundleId = base::apple::BaseBundleID();
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

}  // namespace content
