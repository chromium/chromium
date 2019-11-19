// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "content/common/mac/font_loader.h"
#include "crypto/openssl_util.h"
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "services/service_manager/sandbox/mac/sandbox_mac.h"
#include "services/service_manager/sandbox/switches.h"
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
                         service_manager::SandboxType sandbox_type) {
    std::string profile =
        service_manager::SandboxMac::GetSandboxProfile(sandbox_type) +
        kTempDirSuffix;
    sandbox::SeatbeltExecClient client;
    client.SetProfile(profile);
    SetupSandboxParameters(sandbox_type,
                           *base::CommandLine::ForCurrentProcess(), &client);

    pipe_ = client.GetReadFD();
    ASSERT_GE(pipe_, 0);

    base::LaunchOptions options;
    options.fds_to_remap.push_back(std::make_pair(pipe_, pipe_));

    base::Process process = SpawnChildWithOptions(procname, options);
    ASSERT_TRUE(process.IsValid());
    ASSERT_TRUE(client.SendProfile());

    int rv = -1;
    ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
        process, TestTimeouts::action_timeout(), &rv));
    EXPECT_EQ(0, rv);
  }

  void ExecuteInAllSandboxTypes(const std::string& multiprocess_main,
                                base::RepeatingClosure after_each) {
    constexpr service_manager::SandboxType kSandboxTypes[] = {
        service_manager::SandboxType::SANDBOX_TYPE_AUDIO,
        service_manager::SandboxType::SANDBOX_TYPE_CDM,
        service_manager::SandboxType::SANDBOX_TYPE_GPU,
        service_manager::SandboxType::SANDBOX_TYPE_NACL_LOADER,
        service_manager::SandboxType::SANDBOX_TYPE_PDF_COMPOSITOR,
        service_manager::SandboxType::SANDBOX_TYPE_PPAPI,
        service_manager::SandboxType::SANDBOX_TYPE_RENDERER,
        service_manager::SandboxType::SANDBOX_TYPE_UTILITY,
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
  ExecuteWithParams("RendererWriteProcess",
                    service_manager::SandboxType::SANDBOX_TYPE_RENDERER);
}

MULTIPROCESS_TEST_MAIN(ClipboardAccessProcess) {
  CheckCreateSeatbeltServer();

  std::string pasteboard_name = GetExtraDataValue();
  CHECK(!pasteboard_name.empty());
  CHECK([NSPasteboard pasteboardWithName:base::SysUTF8ToNSString(
                                             pasteboard_name)] == nil);
  CHECK([NSPasteboard generalPasteboard] == nil);

  return 0;
}

TEST_F(SandboxMacTest, ClipboardAccess) {
  scoped_refptr<ui::UniquePasteboard> pb = new ui::UniquePasteboard;
  ASSERT_TRUE(pb->get());
  EXPECT_EQ([[pb->get() types] count], 0U);

  extra_data_ = base::SysNSStringToUTF8([pb->get() name]);

  ExecuteInAllSandboxTypes("ClipboardAccessProcess",
                           base::BindRepeating(
                               [](scoped_refptr<ui::UniquePasteboard> pb) {
                                 ASSERT_EQ([[pb->get() types] count], 0U);
                               },
                               pb));
}

MULTIPROCESS_TEST_MAIN(SSLProcess) {
  CheckCreateSeatbeltServer();

  crypto::EnsureOpenSSLInit();
  // Ensure that RAND_bytes is functional within the sandbox.
  uint8_t byte;
  CHECK(RAND_bytes(&byte, 1) == 1);
  return 0;
}

TEST_F(SandboxMacTest, SSLInitTest) {
  ExecuteInAllSandboxTypes("SSLProcess", base::RepeatingClosure());
}

MULTIPROCESS_TEST_MAIN(FontLoadingProcess) {
  // Create a shared memory handle to mimic what the browser process does.
  std::string font_file_path = GetExtraDataValue();
  CHECK(!font_file_path.empty());

  std::string font_data;
  CHECK(base::ReadFileToString(base::FilePath(font_file_path), &font_data));

  size_t font_data_length = font_data.length();
  CHECK(font_data_length > 0);

  auto font_shmem = mojo::SharedBufferHandle::Create(font_data_length);
  CHECK(font_shmem.is_valid());

  mojo::ScopedSharedBufferMapping mapping = font_shmem->Map(font_data_length);
  CHECK(mapping);

  memcpy(mapping.get(), font_data.c_str(), font_data_length);

  // Now init the sandbox.
  CheckCreateSeatbeltServer();

  mojo::ScopedSharedBufferHandle shmem_handle =
      font_shmem->Clone(mojo::SharedBufferHandle::AccessMode::READ_ONLY);
  CHECK(shmem_handle.is_valid());

  base::ScopedCFTypeRef<CGFontRef> cgfont;
  CHECK(FontLoader::CGFontRefFromBuffer(
      std::move(shmem_handle), font_data_length, cgfont.InitializeInto()));
  CHECK(cgfont);

  base::ScopedCFTypeRef<CTFontRef> ctfont(
      CTFontCreateWithGraphicsFont(cgfont.get(), 16.0, NULL, NULL));
  CHECK(ctfont);

  // Do something with the font to make sure it's loaded.
  CGFloat cap_height = CTFontGetCapHeight(ctfont);
  CHECK(cap_height > 0.0);

  return 0;
}

TEST_F(SandboxMacTest, FontLoadingTest) {
  base::FilePath temp_file_path;
  FILE* temp_file = base::CreateAndOpenTemporaryFile(&temp_file_path);
  ASSERT_TRUE(temp_file);
  base::ScopedFILE temp_file_closer(temp_file);

  std::unique_ptr<FontLoader::ResultInternal> result =
      FontLoader::LoadFontForTesting(base::ASCIIToUTF16("Geeza Pro"), 16);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->font_data.is_valid());
  uint64_t font_data_size = result->font_data->GetSize();
  EXPECT_GT(font_data_size, 0U);
  EXPECT_GT(result->font_id, 0U);

  mojo::ScopedSharedBufferMapping mapping =
      result->font_data->Map(font_data_size);
  ASSERT_TRUE(mapping);

  base::WriteFileDescriptor(fileno(temp_file),
                            static_cast<const char*>(mapping.get()),
                            font_data_size);

  extra_data_ = temp_file_path.value();
  ExecuteWithParams("FontLoadingProcess",
                    service_manager::SandboxType::SANDBOX_TYPE_RENDERER);
  temp_file_closer.reset();
  ASSERT_TRUE(base::DeleteFile(temp_file_path, false));
}

}  // namespace content
