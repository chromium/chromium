# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""String constants for commonly used browser arguments."""

from __future__ import print_function

AUTOPLAY_POLICY_NO_USER_GESTURE_REQUIRED =\
    '--autoplay-policy=no-user-gesture-required'
DISABLE_ACCELERATED_2D_CANVAS = '--disable-accelerated-2d-canvas'
DISABLE_ACCELERATED_VIDEO_DECODE = '--disable-accelerated-video-decode'
DISABLE_DEVICE_DISCOVERY_NOTIFICATIONS = '--disable-features=MediaRouter'
DISABLE_DIRECT_COMPOSITION_VIDEO_OVERLAYS =\
    '--disable_direct_composition_video_overlays=1'
DISABLE_DIRECT_COMPOSITION_VP_SCALING = '--disable_vp_scaling=1'
DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS = '--disable-domain-blocking-for-3d-apis'
DISABLE_D3D11_VIDEO_DECODER = '--disable_d3d11_video_decoder=1'
DISABLE_GPU = '--disable-gpu'
DISABLE_GPU_COMPOSITING = '--disable-gpu-compositing'
DISABLE_GPU_PROCESS_CRASH_LIMIT = '--disable-gpu-process-crash-limit'
DISABLE_SOFTWARE_COMPOSITING_FALLBACK =\
    '--disable-software-compositing-fallback'
DISABLE_SOFTWARE_RASTERIZER = '--disable-software-rasterizer'
ENABLE_DIRECT_COMPOSITION_VIDEO_OVERLAYS =\
    '--enable-direct-composition-video-overlays'
ENABLE_DIRECT_COMPOSITION_VP_SCALING = '--disable_vp_scaling=0'
ENABLE_DXVA_VIDEO_DECODER = '--enable-features=DXVAVideoDecoding'
ENABLE_PLATFORM_HEVC_ENCODER_SUPPORT =\
    '--enable-features=PlatformHEVCEncoderSupport'
ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES =\
    '--enable-experimental-web-platform-features'
ENABLE_GPU_BENCHMARKING = '--enable-gpu-benchmarking'
ENABLE_GPU_RASTERIZATION = '--enable-gpu-rasterization'
ENABLE_LOGGING = '--enable-logging'
ENSURE_FORCED_COLOR_PROFILE = '--ensure-forced-color-profile'
FORCE_BROWSER_CRASH_ON_GPU_CRASH = '--force-browser-crash-on-gpu-crash'
FORCE_COLOR_PROFILE_SRGB = '--force-color-profile=srgb'
TEST_TYPE_GPU = '--test-type=gpu'

# Combinations of flags for specific purpose.
ENABLE_WEBGPU_FOR_TESTING = [
    '--enable-unsafe-webgpu', '--enable-webgpu-developer-features'
]
