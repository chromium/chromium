// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/test/cast_runner_launcher_v2.h"

#include <fuchsia/buildinfo/cpp/fidl.h>
#include <fuchsia/feedback/cpp/fidl.h>
#include <fuchsia/fonts/cpp/fidl.h>
#include <fuchsia/intl/cpp/fidl.h>
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

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "fuchsia_web/runners/cast/cast_runner_switches.h"
#include "fuchsia_web/runners/cast/fidl/fidl/chromium/cast/cpp/fidl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {

namespace {

// Returns a JSON object containing an "args" list of strings to be processed by
// cast_runner as if they were arguments on its command line; see ../main.cc's
// ReadTestConfigData.
std::string SerializeFeatures(CastRunnerFeatures runner_features) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  if (runner_features & kCastRunnerFeaturesHeadless)
    command_line.AppendSwitch(kForceHeadlessForTestsSwitch);
  if (!(runner_features & kCastRunnerFeaturesVulkan))
    command_line.AppendSwitch(kDisableVulkanForTestsSwitch);
  if (runner_features & kCastRunnerFeaturesFrameHost)
    command_line.AppendSwitch(kEnableFrameHostComponentForTestsSwitch);

  base::Value::Dict feature_dict;
  base::Value::List argv_list;
  for (const auto& arg : command_line.argv()) {
    argv_list.Append(arg);
  }
  feature_dict.Set("argv", std::move(argv_list));

  std::string result;
  JSONStringValueSerializer serializer(&result);
  EXPECT_TRUE(serializer.Serialize(feature_dict));
  return result;
}

}  // namespace

CastRunnerLauncherV2::CastRunnerLauncherV2(CastRunnerFeatures runner_features)
    : runner_features_(runner_features) {}

CastRunnerLauncherV2::~CastRunnerLauncherV2() = default;

std::unique_ptr<sys::ServiceDirectory> CastRunnerLauncherV2::StartCastRunner() {
  auto realm_builder = component_testing::RealmBuilder::Create();

  static constexpr char kCastRunnerService[] = "cast_runner";
  realm_builder.AddChild(kCastRunnerService, "#meta/cast_runner.cm");

  // Route capabilities from the fake feedback service to cast_runner.
  static constexpr char kFeedbackService[] = "fake_feedback";
  fake_feedback_service_.emplace();
  realm_builder.AddLocalChild(kFeedbackService,
                              &fake_feedback_service_.value());
  realm_builder.AddRoute(component_testing::Route{
      .capabilities =
          {component_testing::Protocol{
               fuchsia::feedback::ComponentDataRegister::Name_},
           component_testing::Protocol{
               fuchsia::feedback::CrashReportingProductRegister::Name_}},
      .source = component_testing::ChildRef{kFeedbackService},
      .targets = {component_testing::ChildRef{kCastRunnerService}}});

  AddSyslogRoutesFromParent(realm_builder, kCastRunnerService);
  AddVulkanRoutesFromParent(realm_builder, kCastRunnerService);

  // Run an isolated font service for cast_runner.
  AddFontService(realm_builder, kCastRunnerService);

  // Run the test-ui-stack and route the protocols needed by cast_runner to it.
  AddTestUiStack(realm_builder, kCastRunnerService);

  realm_builder.AddRoute(component_testing::Route{
      .capabilities =
          {
              component_testing::Directory{.name = "config-data"},
              component_testing::Protocol{fuchsia::buildinfo::Provider::Name_},
              component_testing::Protocol{
                  fuchsia::intl::PropertyProvider::Name_},
              component_testing::Protocol{
                  fuchsia::media::ProfileProvider::Name_},
              component_testing::Protocol{
                  fuchsia::memorypressure::Provider::Name_},
              component_testing::Protocol{
                  fuchsia::net::interfaces::State::Name_},
              component_testing::Protocol{"fuchsia.posix.socket.Provider"},
              component_testing::Protocol{fuchsia::settings::Display::Name_},
              component_testing::Protocol{fuchsia::sys::Environment::Name_},
              component_testing::Protocol{fuchsia::sys::Loader::Name_},
              component_testing::Storage{.name = "cache", .path = "/cache"},
          },
      .source = component_testing::ParentRef(),
      .targets = {component_testing::ChildRef{kCastRunnerService}}});

  // Route the test config data from the test to the cast_runner.
  component_testing::DirectoryContents config_data_for_testing_directory;
  config_data_for_testing_directory.AddFile(
      "runner-features", SerializeFeatures(runner_features_));
  realm_builder.RouteReadOnlyDirectory(
      "config-data-for-testing",
      {component_testing::ChildRef{kCastRunnerService}},
      std::move(config_data_for_testing_directory));

  // Either route the fake AudioDeviceEnumerator or the system one.
  if (runner_features_ & kCastRunnerFeaturesFakeAudioDeviceEnumerator) {
    static constexpr char kAudioDeviceEnumerator[] =
        "fake_audio_device_enumerator";
    fake_audio_device_enumerator_.emplace();
    realm_builder.AddLocalChild(kAudioDeviceEnumerator,
                                &fake_audio_device_enumerator_.value());
    realm_builder.AddRoute(component_testing::Route{
        .capabilities = {component_testing::Protocol{
            fuchsia::media::AudioDeviceEnumerator::Name_}},
        .source = component_testing::ChildRef{kAudioDeviceEnumerator},
        .targets = {component_testing::ChildRef{kCastRunnerService}}});
  } else {
    realm_builder.AddRoute(component_testing::Route{
        .capabilities = {component_testing::Protocol{
            fuchsia::media::AudioDeviceEnumerator::Name_}},
        .source = component_testing::ParentRef(),
        .targets = {component_testing::ChildRef{kCastRunnerService}}});
  }

  // Route capabilities from the cast_runner back up to the test.
  realm_builder.AddRoute(component_testing::Route{
      .capabilities =
          {component_testing::Protocol{chromium::cast::DataReset::Name_},
           component_testing::Protocol{fuchsia::web::FrameHost::Name_},
           component_testing::Protocol{fuchsia::sys::Runner::Name_}},
      .source = component_testing::ChildRef{kCastRunnerService},
      .targets = {component_testing::ParentRef()}});

  realm_root_ = realm_builder.Build();
  return std::make_unique<sys::ServiceDirectory>(realm_root_->CloneRoot());
}

// static
component_testing::RealmBuilder&
CastRunnerLauncherV2::AddSyslogRoutesFromParent(
    component_testing::RealmBuilder& realm_builder,
    std::string_view child_name) {
  return realm_builder.AddRoute(component_testing::Route{
      .capabilities = {component_testing::Protocol{
          fuchsia::logger::LogSink::Name_}},
      .source = component_testing::ParentRef{},
      .targets = {component_testing::ChildRef{child_name}}});
}

// static
component_testing::RealmBuilder&
CastRunnerLauncherV2::AddVulkanRoutesFromParent(
    component_testing::RealmBuilder& realm_builder,
    std::string_view child_name) {
  return realm_builder.AddRoute(component_testing::Route{
      .capabilities =
          {component_testing::Protocol{fuchsia::sysmem::Allocator::Name_},
           component_testing::Protocol{
               fuchsia::tracing::provider::Registry::Name_},
           component_testing::Protocol{"fuchsia.vulkan.loader.Loader"}},
      .source = component_testing::ParentRef{},
      .targets = {component_testing::ChildRef{child_name}}});
}

// static
component_testing::RealmBuilder& CastRunnerLauncherV2::AddFontService(
    component_testing::RealmBuilder& realm_builder,
    std::string_view child_name) {
  static constexpr char kFontsService[] = "isolated_fonts";
  static constexpr char kFontsUrl[] =
      "fuchsia-pkg://fuchsia.com/fonts#meta/fonts.cm";
  realm_builder.AddChild(kFontsService, kFontsUrl);
  return AddSyslogRoutesFromParent(realm_builder, kFontsService)
      .AddRoute(component_testing::Route{
          .capabilities =
              {
                  component_testing::Directory{.name = "config-data",
                                               .subdir = "fonts"},
              },
          .source = component_testing::ParentRef{},
          .targets = {component_testing::ChildRef{kFontsService}}})
      .AddRoute(component_testing::Route{
          .capabilities =
              {
                  component_testing::Protocol{fuchsia::fonts::Provider::Name_},
              },
          .source = component_testing::ChildRef{kFontsService},
          .targets = {component_testing::ChildRef{child_name}}});
}

// static
component_testing::RealmBuilder& CastRunnerLauncherV2::AddTestUiStack(
    component_testing::RealmBuilder& realm_builder,
    std::string_view child_name) {
  static constexpr char kTestUiStackService[] = "test_ui_stack";
  static constexpr char kTestUiStackUrl[] =
      "fuchsia-pkg://fuchsia.com/test-ui-stack#meta/test-ui-stack.cm";
  realm_builder.AddChild(kTestUiStackService, kTestUiStackUrl);
  AddSyslogRoutesFromParent(realm_builder, kTestUiStackService);
  return AddVulkanRoutesFromParent(realm_builder, kTestUiStackService)
      .AddRoute(component_testing::Route{
          .capabilities =
              {
                  component_testing::Protocol{
                      "fuchsia.scheduler.ProfileProvider"},
              },
          .source = component_testing::ParentRef(),
          .targets = {component_testing::ChildRef{kTestUiStackService}}})
      .AddRoute(component_testing::Route{
          .capabilities =
              {
                  component_testing::Protocol{
                      fuchsia::ui::composition::Allocator::Name_},
                  component_testing::Protocol{
                      fuchsia::ui::scenic::Scenic::Name_},
              },
          .source = component_testing::ChildRef{kTestUiStackService},
          .targets = {component_testing::ChildRef{child_name}}});
}

}  // namespace test
