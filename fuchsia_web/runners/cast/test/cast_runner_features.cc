// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/test/cast_runner_features.h"

#include "base/command_line.h"
#include "fuchsia_web/runners/cast/cast_runner_switches.h"

namespace test {

base::CommandLine CommandLineFromFeatures(CastRunnerFeatures features) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  if (features & kCastRunnerFeaturesHeadless)
    command_line.AppendSwitch(kForceHeadlessForTestsSwitch);
  if (!(features & kCastRunnerFeaturesVulkan))
    command_line.AppendSwitch(kDisableVulkanForTestsSwitch);

  return command_line;
}

}  // namespace test
