// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_SWITCHES_H_
#define FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_SWITCHES_H_

// Disable Vulkan flag for the cast runner. Used for tests.
extern const char kDisableVulkanForTestsSwitch[];

// Run as a CFv2 component, processing StartComponent requests from the CFv1
// shim.
extern const char kEnableCfv2[];

// Force headless mode.
extern const char kForceHeadlessForTestsSwitch[];

#endif  // FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_SWITCHES_H_
