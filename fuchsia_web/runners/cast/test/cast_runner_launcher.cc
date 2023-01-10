// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/test/cast_runner_launcher.h"

#include <fuchsia/buildinfo/cpp/fidl.h>
#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/fonts/cpp/fidl.h>
#include <fuchsia/intl/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/logger/cpp/fidl.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/memorypressure/cpp/fidl.h>
#include <fuchsia/net/interfaces/cpp/fidl.h>
#include <fuchsia/settings/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/tracing/provider/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>

#include "base/command_line.h"
#include "fuchsia_web/common/test/test_realm_support.h"
#include "fuchsia_web/runners/cast/fidl/fidl/chromium/cast/cpp/fidl.h"

using ::component_testing::ChildRef;
using ::component_testing::Directory;
using ::component_testing::DirectoryContents;
using ::component_testing::ParentRef;
using ::component_testing::Protocol;
using ::component_testing::RealmBuilder;
using ::component_testing::Route;
using ::component_testing::Storage;

namespace test {

CastRunnerLauncher::CastRunnerLauncher(CastRunnerFeatures runner_features)
    : runner_features_(runner_features) {}

CastRunnerLauncher::~CastRunnerLauncher() = default;

std::unique_ptr<sys::ServiceDirectory> CastRunnerLauncher::StartCastRunner() {
  auto realm_builder = RealmBuilder::Create();

  static constexpr char kCastRunnerService[] = "cast_runner";
  realm_builder.AddChild(kCastRunnerService, "#meta/cast_runner.cm");

  base::CommandLine command_line = CommandLineFromFeatures(runner_features_);
  constexpr char const* kSwitchesToCopy[] = {"ozone-platform"};
  command_line.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                kSwitchesToCopy, std::size(kSwitchesToCopy));
  AppendCommandLineArguments(realm_builder, kCastRunnerService, command_line);

  // Run the fake fuchsia.feedback service; plumbing its protocols to
  // cast_runner.
  fake_feedback_service_.emplace(realm_builder, kCastRunnerService);

  AddSyslogRoutesFromParent(realm_builder, kCastRunnerService);
  AddVulkanRoutesFromParent(realm_builder, kCastRunnerService);

  // Run an isolated font service for cast_runner.
  AddFontService(realm_builder, kCastRunnerService);

  // Run the test-ui-stack and route the protocols needed by cast_runner to it.
  AddTestUiStack(realm_builder, kCastRunnerService);

  realm_builder.AddRoute(
      Route{.capabilities =
                {
                    Directory{.name = "config-data"},
                    Protocol{fuchsia::buildinfo::Provider::Name_},
                    Protocol{fuchsia::intl::PropertyProvider::Name_},
                    Protocol{fuchsia::media::ProfileProvider::Name_},
                    Protocol{fuchsia::memorypressure::Provider::Name_},
                    Protocol{fuchsia::net::interfaces::State::Name_},
                    Protocol{"fuchsia.posix.socket.Provider"},
                    Protocol{fuchsia::settings::Display::Name_},
                    Protocol{fuchsia::sys::Environment::Name_},
                    Protocol{fuchsia::sys::Loader::Name_},
                    Storage{.name = "cache", .path = "/cache"},
                },
            .source = ParentRef(),
            .targets = {ChildRef{kCastRunnerService}}});

  // Provide a fake Cast "agent", providing some necessary services.
  static constexpr char kFakeCastAgentName[] = "fake-cast-agent";
  fake_cast_agent_.emplace();
  realm_builder.AddLocalChild(kFakeCastAgentName, &fake_cast_agent_.value());
  realm_builder.AddRoute(
      Route{.capabilities =
                {
                    Protocol{chromium::cast::CorsExemptHeaderProvider::Name_},
                    Protocol{fuchsia::camera3::DeviceWatcher::Name_},
                    Protocol{fuchsia::legacymetrics::MetricsRecorder::Name_},
                    Protocol{fuchsia::media::Audio::Name_},
                },
            .source = ChildRef{kFakeCastAgentName},
            .targets = {ChildRef{kCastRunnerService}}});

  // Either route the fake AudioDeviceEnumerator or the system one.
  if (runner_features_ & kCastRunnerFeaturesFakeAudioDeviceEnumerator) {
    static constexpr char kAudioDeviceEnumerator[] =
        "fake_audio_device_enumerator";
    fake_audio_device_enumerator_.emplace();
    realm_builder.AddLocalChild(kAudioDeviceEnumerator,
                                &fake_audio_device_enumerator_.value());
    realm_builder.AddRoute(
        Route{.capabilities = {Protocol{
                  fuchsia::media::AudioDeviceEnumerator::Name_}},
              .source = ChildRef{kAudioDeviceEnumerator},
              .targets = {ChildRef{kCastRunnerService}}});
  } else {
    realm_builder.AddRoute(
        Route{.capabilities = {Protocol{
                  fuchsia::media::AudioDeviceEnumerator::Name_}},
              .source = ParentRef(),
              .targets = {ChildRef{kCastRunnerService}}});
  }

  // Route capabilities from the cast_runner back up to the test.
  realm_builder.AddRoute(
      Route{.capabilities = {Protocol{chromium::cast::DataReset::Name_},
                             Protocol{fuchsia::web::FrameHost::Name_},
                             Protocol{fuchsia::sys::Runner::Name_}},
            .source = ChildRef{kCastRunnerService},
            .targets = {ParentRef()}});

  realm_root_ = realm_builder.Build();
  return std::make_unique<sys::ServiceDirectory>(realm_root_->CloneRoot());
}

}  // namespace test
