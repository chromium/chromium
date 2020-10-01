# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""String constants for commonly used browser arguments."""

AUTOPLAY_POLICY_NO_USER_GESTURE_REQUIRED =\
    '--autoplay-policy=no-user-gesture-required'
DISABLE_ACCELERATED_2D_CANVAS = '--disable-accelerated-2d-canvas'
DISABLE_DEVICE_DISCOVERY_NOTIFICATIONS = '--disable-features=MediaRouter'
DISABLE_DOMAIN_BLOCKING_FOR_3D_APIS = '--disable-domain-blocking-for-3d-apis'
DISABLE_FEATURES_D3D11_VIDEO_DECODER = '--disable-features=D3D11VideoDecoder'
DISABLE_FORCE_FULL_DAMAGE =\
    '--disable-features=DirectCompositionForceFullDamage'
DISABLE_GPU = '--disable-gpu'
DISABLE_GPU_COMPOSITING = '--disable-gpu-compositing'
DISABLE_GPU_PROCESS_CRASH_LIMIT = '--disable-gpu-process-crash-limit'
DISABLE_SOFTWARE_COMPOSITING_FALLBACK =\
    '--disable-software-compositing-fallback'
DISABLE_SOFTWARE_RASTERIZER = '--disable-software-rasterizer'
DISABLE_VP_SCALING = '--disable_vp_scaling=1'
ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES =\
    '--enable-experimental-web-platform-features'
ENABLE_FORCE_FULL_DAMAGE = "--direct-composition-force-full-damage-for-testing"
ENABLE_GPU_BENCHMARKING = '--enable-gpu-benchmarking'
ENSURE_FORCED_COLOR_PROFILE = '--ensure-forced-color-profile'
FORCE_COLOR_PROFILE_SRGB = '--force-color-profile=srgb'
ENABLE_GPU_RASTERIZATION = '--enable-gpu-rasterization'
TEST_TYPE_GPU = '--test-type=gpu'
