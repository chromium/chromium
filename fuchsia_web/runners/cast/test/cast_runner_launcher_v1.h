// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_V1_H_
#define FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_V1_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>

#include <memory>

#include "base/fuchsia/test_component_controller.h"
#include "fuchsia_web/runners/cast/test/cast_runner_features.h"

namespace media {
class FakeAudioDeviceEnumerator;
}

namespace test {

// A launcher for CastRunnerIntegrationTestBase that uses fuchsia.sys.Launcher
// to start the cast runner component. This is for use with the CFv1 variant of
// the integration tests.
class CastRunnerLauncherV1 {
 public:
  // Creates an instance that will launch cast_runner.cmx with the features
  // indicated in the `runner_features` bitmask. This can be used, for example,
  // to provide a fake AudioDeviceEnumerator to the runner.
  explicit CastRunnerLauncherV1(CastRunnerFeatures runner_features);
  CastRunnerLauncherV1(const CastRunnerLauncherV1&) = delete;
  CastRunnerLauncherV1& operator=(const CastRunnerLauncherV1&) = delete;
  ~CastRunnerLauncherV1();

  std::unique_ptr<sys::ServiceDirectory> StartCastRunner();

  ::fuchsia::sys::ComponentControllerPtr& controller_ptr() {
    return controller_.ptr();
  }

  // Returns the outgoing directory for services given to the runner. Services
  // must be added before calling StartCastRunner().
  sys::OutgoingDirectory& services_for_runner() { return services_for_runner_; }

 private:
  const CastRunnerFeatures runner_features_;

  base::TestComponentController controller_;

  // Directory used to publish test ContextProvider to CastRunner. Some tests
  // restart ContextProvider, so we can't pass the services directory from
  // ContextProvider to CastRunner directly.
  sys::OutgoingDirectory services_for_runner_;

  // A fake for fuchsia.media.AudioDeviceEnumerator that is provided to the
  // runner upon request via `runner_features`.
  std::unique_ptr<media::FakeAudioDeviceEnumerator>
      fake_audio_device_enumerator_;
};

}  // namespace test

#endif  // FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_V1_H_
