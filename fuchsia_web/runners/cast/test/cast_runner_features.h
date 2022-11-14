// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_FEATURES_H_
#define FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_FEATURES_H_

#include <stdint.h>

namespace base {
class CommandLine;
}

namespace test {

// A bitfield of feature bits used by cast runner component test launchers.
using CastRunnerFeatures = uint32_t;

// Individual bitmasks for the CastRunnerFeatures bitfield.
enum : uint32_t {
  kCastRunnerFeaturesNone = 0U,
  kCastRunnerFeaturesHeadless = 1U << 0,
  kCastRunnerFeaturesVulkan = 1U << 1,
  kCastRunnerFeaturesFakeAudioDeviceEnumerator = 1U << 2,
};

// Returns a command line for launching cast_runner with the given `features`.
base::CommandLine CommandLineFromFeatures(CastRunnerFeatures features);

}  // namespace test

#endif  // FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_FEATURES_H_
