// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/cfi_buildflags.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/common/shell_controller.test-mojom.h"
#include "content/shell/common/shell_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

#if defined(OS_CHROMEOS)
#include "ui/gl/gl_switches.h"
#endif

namespace content {
namespace {

#if defined(OS_WIN)
const char kShellExecutableName[] = "content_shell.exe";
#else
const char kShellExecutableName[] = "content_shell";
const char kMojoCoreLibraryName[] = "libmojo_core.so";
#endif

const char* kSwitchesToCopy[] = {
#if defined(USE_OZONE)
    // Keep the kOzonePlatform switch that the Ozone must use.
    switches::kOzonePlatform,
#endif
    // Some tests use custom cmdline that doesn't hold switches from previous
    // cmdline. Only a couple of switches are copied. That can result in
    // incorrect initialization of a process. For example, the work that we do
    // to have use_x11 && use_ozone, requires UseOzonePlatform feature flag to
    // be passed to all the process to ensure correct path is chosen.
    // TODO(https://crbug.com/1096425): update this comment once USE_X11 goes
    // away.
    switches::kEnableFeatures,
    switches::kDisableFeatures,
};

base::FilePath GetCurrentDirectory() {
  base::FilePath current_directory;
  CHECK(base::GetCurrentDirectory(&current_directory));
  return current_directory;
}

class LaunchAsMojoClientBrowserTest : public ContentBrowserTest {
 public:
  LaunchAsMojoClientBrowserTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  ~LaunchAsMojoClientBrowserTest() override {
    // Ensure that the launched Content Shell process is dead before the test
    // tears down, otherwise the temp profile dir may fail to delete. Note that
    // tests must explicitly request shutdown through ShellController before
    // finishing, otherwise this will time out.
    CHECK(content_shell_process_.WaitForExit(nullptr));
    CHECK(temp_dir_.Delete());
  }

  base::CommandLine MakeShellCommandLine() {
    base::CommandLine command_line(
        GetFilePathNextToCurrentExecutable(kShellExecutableName));
    command_line.AppendSwitchPath(switches::kContentShellDataPath,
                                  temp_dir_.GetPath());
    const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
    command_line.CopySwitchesFrom(cmdline, kSwitchesToCopy,
                                  base::size(kSwitchesToCopy));

#if defined(OS_CHROMEOS)
    command_line.AppendSwitchASCII(switches::kUseGL, "swiftshader");
#endif
    return command_line;
  }

  mojo::Remote<mojom::ShellController> LaunchContentShell(
      const base::CommandLine& command_line) {
    mojo::PlatformChannel channel;
    base::LaunchOptions options;
    base::CommandLine shell_command_line(command_line);
    channel.PrepareToPassRemoteEndpoint(&options, &shell_command_line);
    content_shell_process_ = base::LaunchProcess(shell_command_line, options);
    channel.RemoteProcessLaunchAttempted();

    mojo::OutgoingInvitation invitation;
    mojo::Remote<mojom::ShellController> controller(
        mojo::PendingRemote<mojom::ShellController>(
            invitation.AttachMessagePipe(0), /*version=*/0));
    mojo::OutgoingInvitation::Send(std::move(invitation),
                                   content_shell_process_.Handle(),
                                   channel.TakeLocalEndpoint());
    return controller;
  }

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  base::FilePath GetMojoCoreLibraryPath() {
    return GetFilePathNextToCurrentExecutable(kMojoCoreLibraryName);
  }
#endif

 private:
  base::FilePath GetFilePathNextToCurrentExecutable(
      const std::string& filename) {
    base::FilePath executable_dir =
        base::CommandLine::ForCurrentProcess()->GetProgram().DirName();
    if (executable_dir.IsAbsolute())
      return executable_dir.AppendASCII(filename);

    // If the current executable path is relative, resolve it to an absolute
    // path before swapping in |filename|. This ensures that the path is OK to
    // use with base::LaunchProcess. Otherwise we could end up with a path
    // containing only |filename|, and this can fail to execute in environments
    // where "." is not in the PATH (common on e.g. Linux).
    return current_directory_.Append(executable_dir).AppendASCII(filename);
  }

  base::ScopedTempDir temp_dir_;
  const base::FilePath current_directory_ = GetCurrentDirectory();
  base::Process content_shell_process_;
  mojo::Remote<mojom::ShellController> shell_controller_;
};

IN_PROC_BROWSER_TEST_F(LaunchAsMojoClientBrowserTest, LaunchAndBindInterface) {
  // Verifies that we can launch an instance of Content Shell with a Mojo
  // invitation on the command line and reach the new browser process's exposed
  // ShellController interface.

  const char kExtraSwitchName[] = "extra-switch-for-testing";
  const char kExtraSwitchValue[] = "42";

  base::CommandLine command_line = MakeShellCommandLine();
  command_line.AppendSwitchASCII(kExtraSwitchName, kExtraSwitchValue);
  mojo::Remote<mojom::ShellController> shell_controller =
      LaunchContentShell(command_line);

  base::RunLoop loop;
  shell_controller->GetSwitchValue(
      kExtraSwitchName,
      base::BindLambdaForTesting([&](const base::Optional<std::string>& value) {
        ASSERT_TRUE(value);
        EXPECT_EQ(kExtraSwitchValue, *value);
        loop.Quit();
      }));
  loop.Run();

  shell_controller->ShutDown();
}

// Running a Content embedder with a dynamically loaded Mojo Core library is
// currently only supported on Linux and Chrome OS.
//
// TODO(crbug.com/1096899): Re-enable on MSan if possible. MSan complains about
// spurious uninitialized memory reads inside base::PlatformThread due to what
// appears to be poor interaction among MSan, PlatformThread's thread_local
// storage, and Mojo's use of dlopen().
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#if defined(MEMORY_SANITIZER) || BUILDFLAG(CFI_ICALL_CHECK)
#define MAYBE_WithMojoCoreLibrary DISABLED_WithMojoCoreLibrary
#else
#define MAYBE_WithMojoCoreLibrary WithMojoCoreLibrary
#endif
IN_PROC_BROWSER_TEST_F(LaunchAsMojoClientBrowserTest,
                       MAYBE_WithMojoCoreLibrary) {
  // Instructs a newly launched Content Shell browser to initialize Mojo Core
  // dynamically from a shared library, rather than using the version linked
  // into the Content Shell binary.
  //
  // This exercises end-to-end JS in order to cover real IPC behavior between
  // the browser and a renderer.

  base::CommandLine command_line = MakeShellCommandLine();
  command_line.AppendSwitchPath(switches::kMojoCoreLibraryPath,
                                GetMojoCoreLibraryPath());
  mojo::Remote<mojom::ShellController> shell_controller =
      LaunchContentShell(command_line);

  // Indisputable proof that we're evaluating JavaScript.
  const std::string kExpressionToEvaluate = "'ba'+ +'a'+'as'";
  const base::Value kExpectedValue("baNaNas");

  base::RunLoop loop;
  shell_controller->ExecuteJavaScript(
      base::ASCIIToUTF16(kExpressionToEvaluate),
      base::BindLambdaForTesting([&](base::Value value) {
        EXPECT_EQ(kExpectedValue, value);
        loop.Quit();
      }));
  loop.Run();

  shell_controller->ShutDown();
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

}  // namespace
}  // namespace content
