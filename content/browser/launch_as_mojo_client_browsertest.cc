// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/cfi_buildflags.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/variations_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/common/shell_controller.test-mojom.h"
#include "content/shell/common/shell_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(MEMORY_SANITIZER)
#include "ui/gl/gl_switches.h"
#endif

namespace content {
namespace {

#if BUILDFLAG(IS_WIN)
const char kShellExecutableName[] = "content_shell.exe";
#else
const char kShellExecutableName[] = "content_shell";
#endif

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
    command_line.AppendSwitchPath(switches::kContentShellUserDataDir,
                                  temp_dir_.GetPath());
#if BUILDFLAG(IS_OZONE)
    const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
    static const char* const kSwitchesToCopy[] = {
        // Keep the kOzonePlatform switch that the Ozone must use.
        switches::kOzonePlatform,
    };
    command_line.CopySwitchesFrom(cmdline, kSwitchesToCopy);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line.AppendSwitchASCII(switches::kUseGL,
                                   gl::kGLImplementationANGLEName);
    command_line.AppendSwitchASCII(switches::kUseANGLE,
                                   gl::kANGLEImplementationSwiftShaderName);
    command_line.AppendSwitch(switches::kEnableUnsafeSwiftShader);
#endif

#if defined(MEMORY_SANITIZER)
    // MSan and GL do not get along so avoid using the GPU with MSan. Normally,
    // BrowserTestBase::SetUp() forces browser tests to use software GL for
    // tests (in both non-MSan and MSan builds), but since this test builds a
    // command line to launch the shell directly, that logic needs to be
    // replicated here.
    command_line.AppendSwitch(switches::kOverrideUseSoftwareGLForTests);
#endif

    const auto& current_command_line = *base::CommandLine::ForCurrentProcess();
    command_line.AppendSwitchASCII(
        switches::kEnableFeatures,
        current_command_line.GetSwitchValueASCII(switches::kEnableFeatures));
    command_line.AppendSwitchASCII(
        switches::kDisableFeatures,
        current_command_line.GetSwitchValueASCII(switches::kDisableFeatures));

    std::string force_field_trials =
        current_command_line.GetSwitchValueASCII(switches::kForceFieldTrials);
    if (!force_field_trials.empty()) {
      command_line.AppendSwitchASCII(switches::kForceFieldTrials,
                                     force_field_trials);

      std::string params =
          base::FieldTrialList::AllParamsToString(variations::EscapeValue);
      if (!params.empty()) {
        command_line.AppendSwitchASCII(
            variations::switches::kForceFieldTrialParams, params);
      }
    }
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

// TODO(http://crbug.com/323984075): This test invokes content_shell in a way
// that is not supported on Lacros (without crosapi data). Figure out what to
// do about that.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
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
      base::BindLambdaForTesting([&](const std::optional<std::string>& value) {
        ASSERT_TRUE(value);
        EXPECT_EQ(kExtraSwitchValue, *value);
        loop.Quit();
      }));
  loop.Run();

  shell_controller->ShutDown();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace
}  // namespace content
