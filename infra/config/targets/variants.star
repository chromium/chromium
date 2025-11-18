# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Variant declarations

Variants are used to expand tests within a matrix compound suite into multiple
tests and applying variant-specific modifications to the tests.
"""

load("@chromium-luci//targets.star", "targets")

targets.variant(
    name = "AMD_RADEON_RX_5500_XT",
    identifier = "AMD Radeon RX 5500 XT",
    mixins = [
        "amd_radeon_rx_5500_xt",
    ],
)

targets.variant(
    name = "CHANNEL_BETA",
    identifier = "Beta Channel",
    args = [
        "--channel=beta",
    ],
)

targets.variant(
    name = "CHANNEL_DEV",
    identifier = "Dev Channel",
    args = [
        "--channel=dev",
    ],
)

targets.variant(
    name = "CHANNEL_STABLE",
    identifier = "Stable Channel",
    args = [
        "--channel=stable",
    ],
)

targets.variant(
    name = "CROS_RELEASE_LKGM",
    identifier = "RELEASE_LKGM",
    skylab = targets.skylab(
        use_lkgm = True,
    ),
)

targets.variant(
    name = "CROS_PUBLIC_LKGM",
    identifier = "PUBLIC_LKGM",
    generate_pyl_entry = False,
    skylab = targets.skylab(
        bucket = "chromiumos-image-archive",
        public_builder = "cros_test_platform_public",
        public_builder_bucket = "testplatform-public",
        use_lkgm = True,
    ),
)

targets.variant(
    name = "CROS_JACUZZI_RELEASE_LKGM",
    identifier = "JACUZZI_RELEASE_LKGM",
    skylab = targets.skylab(
        cros_board = "jacuzzi",
        use_lkgm = True,
    ),
)

targets.variant(
    name = "CROS_GPU_BRYA_RELEASE_LKGM",
    identifier = "GPU_BRYA_RELEASE_LKGM",
    skylab = targets.skylab(
        cros_board = "brya",
        dut_pool = "chrome-gpu",
        use_lkgm = True,
    ),
)

targets.variant(
    name = "CROS_GPU_CORSOLA_RELEASE_LKGM",
    identifier = "GPU_CORSOLA_RELEASE_LKGM",
    skylab = targets.skylab(
        cros_board = "corsola",
        dut_pool = "chrome-gpu",
        use_lkgm = True,
    ),
)

targets.variant(
    name = "CROS_GPU_SKYRIM_RELEASE_LKGM",
    identifier = "GPU_SKYRIM_RELEASE_LKGM",
    skylab = targets.skylab(
        cros_board = "skyrim",
        dut_pool = "chrome-gpu",
        use_lkgm = True,
    ),
)

targets.variant(
    name = "DISABLE_FIELD_TRIAL_CONFIG",
    identifier = "Disable Field Trial Config",
    generate_pyl_entry = False,
    args = [
        "--disable-field-trial-config",
        "--webview-verbose-logging",
    ],
)

targets.variant(
    name = "DISABLE_FIELD_TRIAL_CONFIG_WEBVIEW_COMMANDLINE",
    identifier = "Disable Field Trial Config",
    generate_pyl_entry = False,
    args = [
        "--webview-command-line-arg=--disable-field-trial-config",
        "--webview-command-line-arg=--webview-verbose-logging",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR",
    identifier = "Single Group Per Study Prefer Existing Behavior Field Trial Config",
    generate_pyl_entry = False,
    args = [
        "--variations-test-seed-path=../../components/variations/test_data/cipd/single_group_per_study_prefer_existing_behavior/seed.json",
        "--accept-empty-variations-seed-signature",
        "--webview-verbose-logging",
        "--disable-field-trial-config",
        "--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR",
    identifier = "Single Group Per Study Prefer New Behavior Field Trial Config",
    generate_pyl_entry = False,
    args = [
        "--variations-test-seed-path=../../components/variations/test_data/cipd/single_group_per_study_prefer_new_behavior/seed.json",
        "--accept-empty-variations-seed-signature",
        "--webview-verbose-logging",
        "--disable-field-trial-config",
        "--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR_WEBVIEW_COMMANDLINE",
    identifier = "Single Group Per Study Prefer Existing Behavior Field Trial Config",
    generate_pyl_entry = False,
    args = [
        "--webview-variations-test-seed-path=../../components/variations/test_data/cipd/single_group_per_study_prefer_existing_behavior/seed.json",
        "--webview-command-line-arg=--accept-empty-variations-seed-signature",
        "--webview-command-line-arg=--webview-verbose-logging",
        "--webview-command-line-arg=--disable-field-trial-config",
        "--webview-command-line-arg=--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR_WEBVIEW_COMMANDLINE",
    identifier = "Single Group Per Study Prefer New Behavior Field Trial Config",
    generate_pyl_entry = False,
    args = [
        "--webview-variations-test-seed-path=../../components/variations/test_data/cipd/single_group_per_study_prefer_new_behavior/seed.json",
        "--webview-command-line-arg=--accept-empty-variations-seed-signature",
        "--webview-command-line-arg=--webview-verbose-logging",
        "--webview-command-line-arg=--disable-field-trial-config",
        "--webview-command-line-arg=--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "INTEL_UHD_630_OR_770",
    identifier = "Intel UHD 630 or 770",
    mixins = [
        "intel_uhd_630_or_770",
    ],
)

targets.variant(
    name = "IPHONE_13",
    identifier = "iPhone 13",
    swarming = targets.swarming(
        dimensions = {
            "device": "iPhone14,5",
            "device_status": "available",
        },
    ),
)

targets.variant(
    name = "IPHONE_15_PRO_18",
    identifier = "iPhone 15 Pro 18",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        dimensions = {
            "os": "iOS-18",
            "device": "iPhone16,1",
            "cpu": "x86|arm64",
        },
    ),
)

targets.variant(
    name = "LINUX_INTEL_UHD_630_STABLE",
    identifier = "UHD 630",
    generate_pyl_entry = False,
    mixins = [
        "linux_intel_uhd_630_stable",
    ],
)

targets.variant(
    name = "LINUX_NVIDIA_GTX_1660_STABLE",
    identifier = "GTX 1660",
    generate_pyl_entry = False,
    mixins = [
        "linux_nvidia_gtx_1660_stable",
    ],
)

targets.variant(
    name = "MAC_MINI_INTEL_GPU_STABLE",
    identifier = "8086:3e9b",
    generate_pyl_entry = False,
    mixins = [
        "mac_mini_intel_gpu_stable",
    ],
)

targets.variant(
    name = "MAC_RETINA_AMD_GPU_STABLE",
    identifier = "1002:7340",
    generate_pyl_entry = False,
    mixins = [
        "mac_retina_amd_gpu_stable",
    ],
)

targets.variant(
    name = "NVIDIA_GEFORCE_GTX_1660",
    identifier = "NVIDIA GeForce GTX 1660",
    mixins = [
        "nvidia_geforce_gtx_1660",
    ],
)

targets.variant(
    name = "SIM_APPLE_TV_4K_3RD_GENERATION_26_0",
    identifier = "Apple TV 4K (3rd generation) 26.0",
    generate_pyl_entry = False,
    mixins = [
        "tvos_runtime_cache_26_0",
    ],
    args = [
        "--platform",
        "Apple TV 4K (3rd generation)",
        "--version",
        "26.0",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_5TH_GEN_17_5",
    identifier = "iPad Air (5th generation) 17.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_17_5",
    ],
    args = [
        "--platform",
        "iPad Air (5th generation)",
        "--version",
        "17.5",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_5TH_GEN_18_5",
    identifier = "iPad Air (5th generation) 18.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_5",
    ],
    args = [
        "--platform",
        "iPad Air (5th generation)",
        "--version",
        "18.5",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_6TH_GEN_18_2",
    identifier = "iPad Air (6th generation) 18.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_2",
    ],
    args = [
        "--platform",
        "iPad Air 11-inch (M2)",
        "--version",
        "18.2",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_6TH_GEN_18_5",
    identifier = "iPad Air (6th generation) 18.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_5",
    ],
    args = [
        "--platform",
        "iPad Air 11-inch (M2)",
        "--version",
        "18.5",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_6TH_GEN_26_0",
    identifier = "iPad Air (6th generation) 26.0",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_0",
    ],
    args = [
        "--platform",
        "iPad Air 11-inch (M2)",
        "--version",
        "26.0",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_6TH_GEN_26_2",
    identifier = "iPad Air (6th generation) 26.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_2",
    ],
    args = [
        "--platform",
        "iPad Air 11-inch (M2)",
        "--version",
        "26.2",
    ],
)

targets.variant(
    name = "SIM_IPAD_PRO_7TH_GEN_18_5",
    identifier = "iPad Air (6th generation) 18.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_5",
    ],
    args = [
        "--platform",
        "iPad Air 11-inch (M2)",
        "--version",
        "18.5",
    ],
)

targets.variant(
    name = "SIM_IPAD_PRO_6TH_GEN_17_5",
    identifier = "iPad Pro (12.9-inch) (6th generation) 17.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_17_5",
    ],
    args = [
        "--platform",
        "iPad Pro (12.9-inch) (6th generation)",
        "--version",
        "17.5",
    ],
)

targets.variant(
    name = "SIM_IPAD_PRO_7TH_GEN_18_2",
    identifier = "iPad Pro 13-inch (M4) 18.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_2",
    ],
    args = [
        "--platform",
        "iPad Pro 13-inch (M4)",
        "--version",
        "18.2",
    ],
)

targets.variant(
    name = "SIM_IPAD_PRO_7TH_GEN_26_0",
    identifier = "iPad Pro 13-inch (M4) 26.0",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_0",
    ],
    args = [
        "--platform",
        "iPad Pro 13-inch (M4)",
        "--version",
        "26.0",
    ],
)

targets.variant(
    name = "SIM_IPAD_10TH_GEN_17_5",
    identifier = "iPad (10th generation) 17.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_17_5",
    ],
    args = [
        "--platform",
        "iPad (10th generation)",
        "--version",
        "17.5",
    ],
)

targets.variant(
    name = "SIM_IPAD_10TH_GEN_18_2",
    identifier = "iPad (10th generation) 18.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_2",
    ],
    args = [
        "--platform",
        "iPad (10th generation)",
        "--version",
        "18.2",
    ],
)

targets.variant(
    name = "SIM_IPAD_10TH_GEN_18_5",
    identifier = "iPad (10th generation) 18.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_5",
    ],
    args = [
        "--platform",
        "iPad (10th generation)",
        "--version",
        "18.5",
    ],
)

targets.variant(
    name = "SIM_IPAD_10TH_GEN_26_0",
    identifier = "iPad (10th generation) 26.0",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_0",
    ],
    args = [
        "--platform",
        "iPad (10th generation)",
        "--version",
        "26.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_17_5",
    identifier = "iPhone 14 17.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_17_5",
    ],
    args = [
        "--platform",
        "iPhone 14",
        "--version",
        "17.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_18_2",
    identifier = "iPhone 14 18.2",
    mixins = [
        "ios_runtime_cache_18_2",
    ],
    args = [
        "--platform",
        "iPhone 14",
        "--version",
        "18.2",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_18_5",
    identifier = "iPhone 14 18.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_5",
    ],
    args = [
        "--platform",
        "iPhone 14",
        "--version",
        "18.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_15_18_2",
    identifier = "iPhone 15 18.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_2",
    ],
    args = [
        "--platform",
        "iPhone 15",
        "--version",
        "18.2",
    ],
)

targets.variant(
    name = "SIM_IPHONE_15_26_0",
    identifier = "iPhone 15 26.0",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_0",
    ],
    args = [
        "--platform",
        "iPhone 15",
        "--version",
        "26.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_15_26_2",
    identifier = "iPhone 15 26.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_2",
    ],
    args = [
        "--platform",
        "iPhone 15",
        "--version",
        "26.2",
    ],
)

targets.variant(
    name = "SIM_IPHONE_15_18_5",
    identifier = "iPhone 15 18.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_5",
    ],
    args = [
        "--platform",
        "iPhone 15",
        "--version",
        "18.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_16_26_0",
    identifier = "iPhone 16 26.0",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_0",
    ],
    args = [
        "--platform",
        "iPhone 16",
        "--version",
        "26.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_16_26_2",
    identifier = "iPhone 16 26.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_2",
    ],
    args = [
        "--platform",
        "iPhone 16",
        "--version",
        "26.2",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_PLUS_17_5",
    identifier = "iPhone 14 Plus 17.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_17_5",
    ],
    args = [
        "--platform",
        "iPhone 14 Plus",
        "--version",
        "17.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_PLUS_18_2",
    identifier = "iPhone 14 Plus 18.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_2",
    ],
    args = [
        "--platform",
        "iPhone 14 Plus",
        "--version",
        "18.2",
    ],
)

targets.variant(
    name = "SIM_IPHONE_16_PLUS_26_0",
    identifier = "iPhone 16 Plus 26.0",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_0",
    ],
    args = [
        "--platform",
        "iPhone 16 Plus",
        "--version",
        "26.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_PRO_MAX_17_5",
    identifier = "iPhone 14 Pro Max 17.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_17_5",
    ],
    args = [
        "--platform",
        "iPhone 14 Pro Max",
        "--version",
        "17.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_15_PRO_MAX_18_2",
    identifier = "iPhone 15 Pro Max 18.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_2",
    ],
    args = [
        "--platform",
        "iPhone 15 Pro Max",
        "--version",
        "18.2",
    ],
)

targets.variant(
    name = "SIM_IPHONE_15_PRO_MAX_18_5",
    identifier = "iPhone 15 Pro Max 18.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_5",
    ],
    args = [
        "--platform",
        "iPhone 15 Pro Max",
        "--version",
        "18.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_SE_3RD_GEN_17_5",
    identifier = "iPhone SE (3rd generation) 17.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_17_5",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "17.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_SE_3RD_GEN_18_2",
    identifier = "iPhone SE (3rd generation) 18.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_2",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "18.2",
    ],
)

targets.variant(
    name = "SIM_IPHONE_SE_3RD_GEN_18_5",
    identifier = "iPhone SE (3rd generation) 18.5",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_5",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "18.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_SE_3RD_GEN_26_0",
    identifier = "iPhone SE (3rd generation) 26.0",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_0",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "26.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_SE_3RD_GEN_26_2",
    identifier = "iPhone SE (3rd generation) 26.2",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_26_2",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "26.2",
    ],
)

targets.variant(
    name = "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
    identifier = "full_mode",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        shards = 2,
    ),
)

targets.variant(
    name = "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
    identifier = "instant_mode",
    generate_pyl_entry = False,
    args = [
        "--exclude-annotation",
        "AppModeFull",
        "--test-apk-as-instant",
    ],
)

targets.variant(
    name = "WIN10_INTEL_UHD_630_STABLE",
    identifier = "8086:9bc5",
    generate_pyl_entry = False,
    mixins = [
        "swarming_containment_auto",
        "win10_intel_uhd_630_stable",
    ],
)

targets.variant(
    name = "WIN10_NVIDIA_GTX_1660_STABLE",
    identifier = "10de:2184",
    generate_pyl_entry = False,
    mixins = [
        "win10_nvidia_gtx_1660_stable",
    ],
)
