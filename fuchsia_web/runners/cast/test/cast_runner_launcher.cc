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

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "fuchsia_web/common/test/test_realm_support.h"
#include "fuchsia_web/runners/cast/fidl/fidl/hlcpp/chromium/cast/cpp/fidl.h"
#include "media/fuchsia/audio/fake_audio_device_enumerator_local_component.h"

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

CastRunnerLauncher::~CastRunnerLauncher() {
  if (realm_root_.has_value()) {
    base::RunLoop run_loop;
    realm_root_.value().Teardown(
        [quit = run_loop.QuitClosure()](auto result) { quit.Run(); });
    run_loop.Run();
  }
}

std::unique_ptr<sys::ServiceDirectory> CastRunnerLauncher::StartCastRunner() {
  auto realm_builder = RealmBuilder::Create();

  static constexpr char kCastRunnerService[] = "cast_runner";
  realm_builder.AddChild(kCastRunnerService, "#meta/cast_runner.cm");

  base::CommandLine command_line = CommandLineFromFeatures(runner_features_);
  constexpr char const* kSwitchesToCopy[] = {"ozone-platform"};
  command_line.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                kSwitchesToCopy, std::size(kSwitchesToCopy));
  AppendCommandLineArguments(realm_builder, kCastRunnerService, command_line);

  // Register the fake fuchsia.feedback service component; plumbing its
  // protocols to cast_runner.
  FakeFeedbackService::RouteToChild(realm_builder, kCastRunnerService);

  AddSyslogRoutesFromParent(realm_builder, kCastRunnerService);
  AddVulkanRoutesFromParent(realm_builder, kCastRunnerService);

  // Run an isolated font service for cast_runner.
  AddFontService(realm_builder, kCastRunnerService);

  // Run the test-ui-stack and route the protocols needed by cast_runner to it.
  AddTestUiStack(realm_builder, kCastRunnerService);

  realm_builder.AddRoute(Route{
      .capabilities =
          {
              // The chromium test realm offers the system-wide config-data dir
              // to test components. Route the cast_runner sub-directory of this
              // to the launched cast_runner component.
              Directory{.name = "config-data", .subdir = "cast_runner"},
              // And route the web_engine sub-directory as required by
              // WebInstanceHost.
              Directory{.name = "config-data",
                        .as = "config-data-for-web-instance",
                        .subdir = "web_engine"},
              Directory{.name = "root-ssl-certificates"},
              Protocol{fuchsia::buildinfo::Provider::Name_},
              Protocol{fuchsia::intl::PropertyProvider::Name_},
              Protocol{fuchsia::media::ProfileProvider::Name_},
              Protocol{fuchsia::memorypressure::Provider::Name_},
              Protocol{fuchsia::net::interfaces::State::Name_},
              Protocol{"fuchsia.posix.socket.Provider"},
              Protocol{"fuchsia.process.Launcher"},
              Protocol{fuchsia::settings::Display::Name_},
              Protocol{fuchsia::sys::Environment::Name_},
              Protocol{fuchsia::sys::Loader::Name_},
              Storage{.name = "cache", .path = "/cache"},
          },
      .source = ParentRef(),
      .targets = {ChildRef{kCastRunnerService}}});

  // Provide a fake Cast "agent", providing some necessary services.
  static constexpr char kFakeCastAgentName[] = "fake-cast-agent";
  auto fake_cast_agent = std::make_unique<FakeCastAgent>();
  fake_cast_agent_ = fake_cast_agent.get();
  realm_builder.AddLocalChild(
      kFakeCastAgentName,
      [fake_cast_agent = std::move(fake_cast_agent)]() mutable {
        return std::move(fake_cast_agent);
      });
  realm_builder.AddRoute(
      Route{.capabilities =
                {
                    Protocol{chromium::cast::ApplicationConfigManager::Name_},
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
    realm_builder.AddLocalChild(kAudioDeviceEnumerator, []() {
      return std::make_unique<media::FakeAudioDeviceEnumeratorLocalComponent>();
    });
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
  return std::make_unique<sys::ServiceDirectory>(
      realm_root_->component().CloneExposedDir());
}

}  // namespace test
