// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_FEATURES_H_
#define FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_FEATURES_H_

namespace test {

// A bitfield of feature bits used by cast runner component test launchers.
using CastRunnerFeatures = uint32_t;

// Individual bitmasks for the CastRunnerFeatures bitfield.
enum : uint32_t {
  kCastRunnerFeaturesNone = 0U,
  kCastRunnerFeaturesHeadless = 1U << 0,
  kCastRunnerFeaturesVulkan = 1U << 1,
  kCastRunnerFeaturesFrameHost = 1U << 2,
  kCastRunnerFeaturesFakeAudioDeviceEnumerator = 1U << 3,
#if defined(USE_CFV1_LAUNCHER)
  kCastRunnerFeaturesCfv1Shim = 1U << 4,
#endif
};

}  // namespace test

#endif  // FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_FEATURES_H_
