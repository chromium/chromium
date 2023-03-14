// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_switches.h"

namespace switches {

// Disable GPU rasterization, i.e. rasterize on the CPU only.
// Overrides the kEnableGpuRasterization flag.
const char kDisableGpuRasterization[] = "disable-gpu-rasterization";

// Disables mipmap generation in Skia. Used a workaround for select low memory
// devices, see https://crbug.com/1138979 for details.
const char kDisableMipmapGeneration[] = "disable-mipmap-generation";

// Allow heuristics to determine when a layer tile should be drawn with the
// Skia GPU backend. Only valid with GPU accelerated compositing.
const char kEnableGpuRasterization[] = "enable-gpu-rasterization";

// Select a different set of GPU blocklist entries with the specified
// test_group ID.
const char kGpuBlocklistTestGroup[] = "gpu-blocklist-test-group";

// Enable an extra set of GPU driver bug list entries with the specified
// test_group ID. Note the default test group (group 0) is still active.
const char kGpuDriverBugListTestGroup[] = "gpu-driver-bug-list-test-group";

// Passes encoded GpuPreferences to GPU process.
const char kGpuPreferences[] = "gpu-preferences";

// Ignores GPU blocklist.
const char kIgnoreGpuBlocklist[] = "ignore-gpu-blocklist";

// Allows explicitly specifying the shader disk cache size for embedded devices.
// Default value is 6MB. On Android, 2MB is default and 128KB for low-end
// devices.
const char kGpuDiskCacheSizeKB[] = "gpu-disk-cache-size-kb";

// Disables the non-sandboxed GPU process for DX12 info collection
const char kDisableGpuProcessForDX12InfoCollection[] =
    "disable-gpu-process-for-dx12-info-collection";

const char kEnableUnsafeWebGPU[] = "enable-unsafe-webgpu";

// Enables WebGPU developer features which are not generally exposed to the web
// platform.
const char kEnableWebGPUDeveloperFeatures[] =
    "enable-webgpu-developer-features";

// Enable validation layers in Dawn backends.
const char kEnableDawnBackendValidation[] = "enable-dawn-backend-validation";

// The adapter to use for WebGPU content.
GPU_EXPORT extern const char kUseWebGPUAdapter[] = "use-webgpu-adapter";

// Set the Dawn features(toggles) enabled on the creation of Dawn devices.
const char kEnableDawnFeatures[] = "enable-dawn-features";

// Set the Dawn features(toggles) disabled on the creation of Dawn devices.
const char kDisableDawnFeatures[] = "disable-dawn-features";

// Changes the type (to kRealtimeAudio) of gpu process and compositor thread.
// This is only to be used for perf tests on macOS for more reliable values.
const char kUseHighGPUThreadPriorityForPerfTests[] =
    "use-gpu-high-thread-priority-for-perf-tests";

// Start the non-sandboxed GPU process for DX12 and Vulkan info collection
// immediately after the browser starts. The default is to delay for 120
// seconds.
const char kNoDelayForDX12VulkanInfoCollection[] =
    "no-delay-for-dx12-vulkan-info-collection";

// Enables measures of how long GPU Main Thread was blocked between SwapBuffers
const char kEnableGpuBlockedTime[] = "enable-gpu-blocked-time";

// Passes the active graphics vendor id from browser process to info collection
// GPU process.
const char kGpuVendorId[] = "gpu-vendor-id";

// Passes the active graphics device id from browser process to info collection
// GPU process.
const char kGpuDeviceId[] = "gpu-device-id";

// Passes the active graphics sub system id from browser process to info
// collection GPU process.
const char kGpuSubSystemId[] = "gpu-sub-system-id";

// Passes the active graphics revision info from browser process to info
// collection GPU process.
const char kGpuRevision[] = "gpu-revision";

// Passes the active graphics driver version from browser process to info
// collection GPU process.
const char kGpuDriverVersion[] = "gpu-driver-version";

// Indicate that the this is being used by Android WebView and its draw functor
// is using vulkan.
const char kWebViewDrawFunctorUsesVulkan[] = "webview-draw-functor-uses-vulkan";

// Enables using protected memory for vulkan resources.
const char kEnableVulkanProtectedMemory[] = "enable-vulkan-protected-memory";

// Disables falling back to GL based hardware rendering if initializing Vulkan
// fails. This is to allow tests to catch regressions in Vulkan.
const char kDisableVulkanFallbackToGLForTesting[] =
    "disable-vulkan-fallback-to-gl-for-testing";

// Specifies the heap limit for Vulkan memory.
// TODO(crbug/1158000): Remove this switch.
const char kVulkanHeapMemoryLimitMb[] = "vulkan-heap-memory-limit-mb";

// Specifies the sync CPU limit for total Vulkan memory.
// TODO(crbug/1158000): Remove this switch.
const char kVulkanSyncCpuMemoryLimitMb[] = "vulkan-sync-cpu-memory-limit-mb";

// Crash Chrome if GPU process crashes. This is to force a test to fail when
// GPU process crashes unexpectedly.
const char kForceBrowserCrashOnGpuCrash[] = "force-browser-crash-on-gpu-crash";

// Override value for the GPU watchdog timeout in seconds.
const char kGpuWatchdogTimeoutSeconds[] = "gpu-watchdog-timeout-seconds";

// Force the use of a separate EGL display for WebGL contexts. Used for testing
// multi-GPU pathways on devices with only one valid GPU.
const char kForceSeparateEGLDisplayForWebGLTesting[] =
    "force-separate-egl-display-for-webgl-testing";

}  // namespace switches
