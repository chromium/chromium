// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_switches.h"

namespace switches {

// Disable workarounds for various GPU driver bugs.
const char kDisableGpuDriverBugWorkarounds[] =
    "disable-gpu-driver-bug-workarounds";

// Disable GPU rasterization, i.e. rasterize on the CPU only.
// Overrides the kEnableGpuRasterization and kForceGpuRasterization flags.
const char kDisableGpuRasterization[] = "disable-gpu-rasterization";

// Allow heuristics to determine when a layer tile should be drawn with the
// Skia GPU backend. Only valid with GPU accelerated compositing +
// impl-side painting.
const char kEnableGpuRasterization[] = "enable-gpu-rasterization";

// Select a different set of GPU blacklist entries with the specificed
// test_group ID.
const char kGpuBlacklistTestGroup[] = "gpu-blacklist-test-group";

// Enable an extra set of GPU driver bug list entries with the specified
// test_group ID. Note the default test group (group 0) is still active.
const char kGpuDriverBugListTestGroup[] = "gpu-driver-bug-list-test-group";

// Passes encoded GpuPreferences to GPU process.
const char kGpuPreferences[] = "gpu-preferences";

// Ignores GPU blacklist.
const char kIgnoreGpuBlacklist[] = "ignore-gpu-blacklist";

// Allows user to override maximum number of active WebGL contexts per
// renderer process.
const char kMaxActiveWebGLContexts[] = "max-active-webgl-contexts";

// Allows explicitly specifying the shader disk cache size for embedded devices.
// Default value is 6MB. On Android, 2MB is default and 128KB for low-end
// devices.
const char kShaderDiskCacheSizeKB[] = "shader-disk-cache-size-kb";

// Set the antialiasing method used for webgl. (none, explicit, implicit, or
// screenspace)
const char kWebglAntialiasingMode[] = "webgl-antialiasing-mode";

// Set a default sample count for webgl if msaa is enabled.
const char kWebglMSAASampleCount[] = "webgl-msaa-sample-count";

// Disables the non-sandboxed GPU process for DX12 and Vulkan info collection
const char kDisableGpuProcessForDX12VulkanInfoCollection[] =
    "disable-gpu-process-for-dx12-vulkan-info-collection";

const char kEnableUnsafeWebGPU[] = "enable-unsafe-webgpu";

}  // namespace switches
