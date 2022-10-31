// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_H_
#define FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_H_

#include <lib/sys/component/cpp/testing/realm_builder.h>
#include <lib/sys/cpp/service_directory.h>

#include <memory>
#include <string_view>

#include "fuchsia_web/common/test/fake_feedback_service.h"
#include "fuchsia_web/runners/cast/test/cast_runner_features.h"
#include "fuchsia_web/runners/cast/test/fake_cast_agent.h"
#include "media/fuchsia/audio/fake_audio_device_enumerator_local_component.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace test {

// A launcher for CastRunnerIntegrationTestBase that uses
// component_testing.RealmBuilder to start the cast runner component.
class CastRunnerLauncher {
 public:
  explicit CastRunnerLauncher(CastRunnerFeatures runner_features);
  CastRunnerLauncher(const CastRunnerLauncher&) = delete;
  CastRunnerLauncher& operator=(const CastRunnerLauncher&) = delete;
  ~CastRunnerLauncher();

  std::unique_ptr<sys::ServiceDirectory> StartCastRunner();

  // May only be called after StartCastRunner().
  FakeCastAgent& fake_cast_agent() { return *fake_cast_agent_; }

 private:
  const CastRunnerFeatures runner_features_;

  absl::optional<media::FakeAudioDeviceEnumeratorLocalComponent>
      fake_audio_device_enumerator_;
  absl::optional<FakeCastAgent> fake_cast_agent_;
  absl::optional<FakeFeedbackService> fake_feedback_service_;

  absl::optional<::component_testing::RealmRoot> realm_root_;
};

}  // namespace test

#endif  // FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_H_
