// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/test/cast_runner_launcher_v1.h"

#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fdio/fd.h>
#include <lib/sys/cpp/component_context.h>
#include <unistd.h>
#include <zircon/processargs.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "fuchsia_web/runners/cast/cast_runner_switches.h"
#include "media/fuchsia/audio/fake_audio_device_enumerator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kEnableCfv1Shim[] = "enable-cfv1-shim";

}  // namespace

namespace test {

CastRunnerLauncherV1::CastRunnerLauncherV1(CastRunnerFeatures runner_features)
    : runner_features_(runner_features) {}

CastRunnerLauncherV1::~CastRunnerLauncherV1() = default;

std::unique_ptr<sys::ServiceDirectory> CastRunnerLauncherV1::StartCastRunner() {
  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url =
      "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cmx";

  // Clone stderr from the current process to CastRunner and ask it to
  // redirect all logs to stderr.
  launch_info.err = fuchsia::sys::FileDescriptor::New();
  launch_info.err->type0 = PA_FD;
  zx_status_t status = fdio_fd_clone(
      STDERR_FILENO, launch_info.err->handle0.reset_and_get_address());
  ZX_CHECK(status == ZX_OK, status);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII("enable-logging", "stderr");

  if (runner_features_ & kCastRunnerFeaturesHeadless)
    command_line.AppendSwitch(kForceHeadlessForTestsSwitch);
  if (!(runner_features_ & kCastRunnerFeaturesVulkan))
    command_line.AppendSwitch(kDisableVulkanForTestsSwitch);
  if (runner_features_ & kCastRunnerFeaturesFrameHost)
    command_line.AppendSwitch(kEnableFrameHostComponentForTestsSwitch);
  if (runner_features_ & kCastRunnerFeaturesCfv1Shim)
    command_line.AppendSwitch(kEnableCfv1Shim);

  // Add all switches and arguments, skipping the program.
  launch_info.arguments.emplace(std::vector<std::string>(
      command_line.argv().begin() + 1, command_line.argv().end()));

  auto additional_services = std::make_unique<fuchsia::sys::ServiceList>();
  auto* svc_dir = services_for_runner().GetOrCreateDirectory("svc");
  if (runner_features_ & kCastRunnerFeaturesFakeAudioDeviceEnumerator) {
    fake_audio_device_enumerator_ =
        std::make_unique<media::FakeAudioDeviceEnumerator>(svc_dir);
    additional_services->names.push_back(
        fuchsia::media::AudioDeviceEnumerator::Name_);
  }
  if (runner_features_ & kCastRunnerFeaturesCfv1Shim) {
    additional_services->names.push_back("fuchsia.sys.Runner-cast");
  }

  fuchsia::io::DirectoryHandle svc_dir_handle;
  svc_dir->Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                     fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                 svc_dir_handle.NewRequest().TakeChannel());
  additional_services->host_directory = svc_dir_handle.TakeChannel();

  launch_info.additional_services = std::move(additional_services);

  fuchsia::io::DirectoryHandle cast_runner_services_dir;
  launch_info.directory_request =
      cast_runner_services_dir.NewRequest().TakeChannel();

  fuchsia::sys::LauncherPtr launcher;
  base::ComponentContextForProcess()->svc()->Connect(launcher.NewRequest());
  launcher->CreateComponent(std::move(launch_info),
                            controller_.ptr().NewRequest());
  return std::make_unique<sys::ServiceDirectory>(
      std::move(cast_runner_services_dir));
}

}  // namespace test
