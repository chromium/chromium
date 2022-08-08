// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_V2_H_
#define FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_V2_H_

#include <lib/sys/component/cpp/testing/realm_builder.h>
#include <lib/sys/cpp/service_directory.h>

#include <memory>
#include <string_view>

#include "fuchsia_web/runners/cast/test/cast_runner_features.h"
#include "fuchsia_web/runners/cast/test/fake_feedback_service.h"
#include "media/fuchsia/audio/fake_audio_device_enumerator_local_component.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace test {

// A launcher for CastRunnerIntegrationTestBase that uses
// component_testing.RealmBuilder to start the cast runner component. This is
// for use with the CFv2 variant of the integration tests.
class CastRunnerLauncherV2 {
 public:
  explicit CastRunnerLauncherV2(CastRunnerFeatures runner_features);
  CastRunnerLauncherV2(const CastRunnerLauncherV2&) = delete;
  CastRunnerLauncherV2& operator=(const CastRunnerLauncherV2&) = delete;
  ~CastRunnerLauncherV2();

  std::unique_ptr<sys::ServiceDirectory> StartCastRunner();

 private:
  // Adds routes to the child component named `child_name` to satisfy that
  // child's use of syslog/client.shard.cml.
  static component_testing::RealmBuilder& AddSyslogRoutesFromParent(
      component_testing::RealmBuilder& realm_builder,
      std::string_view child_name);

  // Adds routes to the child component named `child_name` to satisfy that
  // child's use of vulkan/client.shard.cml.
  static component_testing::RealmBuilder& AddVulkanRoutesFromParent(
      component_testing::RealmBuilder& realm_builder,
      std::string_view child_name);

  // Adds fuchsia-pkg://fuchsia.com/fonts#meta/fonts.cm as a child in the realm,
  // routes all of its required capabilities from parent, and routes its
  // fuchsia.fonts.Provider protocol to the child component named `child_name`
  // in the realm.
  static component_testing::RealmBuilder& AddFontService(
      component_testing::RealmBuilder& realm_builder,
      std::string_view child_name);

  // Adds fuchsia-pkg://fuchsia.com/test-ui-stack#meta/test-ui-stack.cm as a
  // child in the realm, routes all of its required capabilities from parent,
  // and routes various of its protocols to the child component named
  // `child_name` in the realm.
  static component_testing::RealmBuilder& AddTestUiStack(
      component_testing::RealmBuilder& realm_builder,
      std::string_view child_name);

  const CastRunnerFeatures runner_features_;
  absl::optional<FakeFeedbackService> fake_feedback_service_;
  absl::optional<media::FakeAudioDeviceEnumeratorLocalComponent>
      fake_audio_device_enumerator_;
  absl::optional<component_testing::RealmRoot> realm_root_;
};

}  // namespace test

#endif  // FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_V2_H_
