# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/targets.star", "targets")

targets.variant(
    name = "AMD_RADEON_RX_5500_XT",
    identifier = "AMD Radeon RX 5500 XT",
    mixins = [
        "amd_radeon_rx_5500_xt",
    ],
)

targets.variant(
    name = "DISABLE_FIELD_TRIAL_CONFIG",
    identifier = "Disable Field Trial Config",
    args = [
        "--disable-field-trial-config",
        "--webview-verbose-logging",
    ],
)

targets.variant(
    name = "DISABLE_FIELD_TRIAL_CONFIG_WEBVIEW_COMMANDLINE",
    identifier = "Disable Field Trial Config",
    args = [
        "--webview-command-line-arg=--disable-field-trial-config",
        "--webview-command-line-arg=--webview-verbose-logging",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR",
    identifier = "Single Group Per Study Prefer Existing Behavior Field Trial Config",
    args = [
        "--variations-test-seed-path=../../third_party/chromium-variations/single_group_per_study_prefer_existing_behavior/seed.json",
        "--accept-empty-variations-seed-signature",
        "--webview-verbose-logging",
        "--disable-field-trial-config",
        "--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR",
    identifier = "Single Group Per Study Prefer New Behavior Field Trial Config",
    args = [
        "--variations-test-seed-path=../../third_party/chromium-variations/single_group_per_study_prefer_new_behavior/seed.json",
        "--accept-empty-variations-seed-signature",
        "--webview-verbose-logging",
        "--disable-field-trial-config",
        "--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR_WEBVIEW_COMMANDLINE",
    identifier = "Single Group Per Study Prefer Existing Behavior Field Trial Config",
    args = [
        "--webview-variations-test-seed-path=../../third_party/chromium-variations/single_group_per_study_prefer_existing_behavior/seed.json",
        "--webview-command-line-arg=--accept-empty-variations-seed-signature",
        "--webview-command-line-arg=--webview-verbose-logging",
        "--webview-command-line-arg=--disable-field-trial-config",
        "--webview-command-line-arg=--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR_WEBVIEW_COMMANDLINE",
    identifier = "Single Group Per Study Prefer New Behavior Field Trial Config",
    args = [
        "--webview-variations-test-seed-path=../../third_party/chromium-variations/single_group_per_study_prefer_new_behavior/seed.json",
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
    name = "IPHONE_15_PRO_18_0",
    identifier = "iPhone 15 Pro 17.5.1 or 17.6.1 or 18.0",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        dimensions = {
            "os": "iOS-17.5.1|iOS-17.6.1|iOS-18.0",
            "device": "iPhone16,1",
        },
    ),
)

targets.variant(
    name = "LINUX_INTEL_UHD_630_STABLE",
    identifier = "UHD 630",
    mixins = [
        "linux_intel_uhd_630_stable",
    ],
)

targets.variant(
    name = "LINUX_NVIDIA_GTX_1660_STABLE",
    identifier = "GTX 1660",
    mixins = [
        "linux_nvidia_gtx_1660_stable",
    ],
)

targets.variant(
    name = "MAC_MINI_INTEL_GPU_STABLE",
    identifier = "8086:3e9b",
    mixins = [
        "mac_mini_intel_gpu_stable",
    ],
)

targets.variant(
    name = "MAC_RETINA_AMD_GPU_STABLE",
    identifier = "1002:7340",
    mixins = [
        "mac_retina_amd_gpu_stable",
    ],
)

targets.variant(
    name = "MAC_RETINA_NVIDIA_GPU_STABLE",
    identifier = "10de:0fe9",
    mixins = [
        "mac_retina_nvidia_gpu_stable",
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
    name = "SIM_IPAD_AIR_5TH_GEN_16_4",
    identifier = "iPad Air (5th generation) 16.4",
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPad Air (5th generation)",
        "--version",
        "16.4",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_5TH_GEN_17_5",
    identifier = "iPad Air (5th generation) 17.5",
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
    name = "SIM_IPAD_AIR_5TH_GEN_18_1",
    identifier = "iPad Air (5th generation) 18.1",
    mixins = [
        "ios_runtime_cache_18_1",
    ],
    args = [
        "--platform",
        "iPad Air (5th generation)",
        "--version",
        "18.1",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_6TH_GEN_18_0",
    identifier = "iPad Air (6th generation) 18.0",
    mixins = [
        "ios_runtime_cache_18_0",
    ],
    args = [
        "--platform",
        "iPad Air 11-inch (M2)",
        "--version",
        "18.0",
    ],
)

targets.variant(
    name = "SIM_IPAD_PRO_6TH_GEN_16_4",
    identifier = "iPad Pro (12.9-inch) (6th generation) 16.4",
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPad Pro (12.9-inch) (6th generation)",
        "--version",
        "16.4",
    ],
)

targets.variant(
    name = "SIM_IPAD_PRO_6TH_GEN_17_5",
    identifier = "iPad Pro (12.9-inch) (6th generation) 17.5",
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
    name = "SIM_IPAD_PRO_7TH_GEN_18_0",
    identifier = "iPad Pro 13-inch (M4) 18.0",
    mixins = [
        "ios_runtime_cache_18_0",
    ],
    args = [
        "--platform",
        "iPad Pro 13-inch (M4)",
        "--version",
        "18.0",
    ],
)

targets.variant(
    name = "SIM_IPAD_10TH_GEN_18_0",
    identifier = "iPad (10th generation) 18.0",
    mixins = [
        "ios_runtime_cache_18_0",
    ],
    args = [
        "--platform",
        "iPad (10th generation)",
        "--version",
        "18.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_16_4",
    identifier = "iPhone 14 16.4",
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPhone 14",
        "--version",
        "16.4",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_17_5",
    identifier = "iPhone 14 17.5",
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
    name = "SIM_IPHONE_14_18_0",
    identifier = "iPhone 14 18.0",
    mixins = [
        "ios_runtime_cache_18_0",
    ],
    args = [
        "--platform",
        "iPhone 14",
        "--version",
        "18.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_18_1",
    identifier = "iPhone 14 18.1",
    mixins = [
        "ios_runtime_cache_18_1",
    ],
    args = [
        "--platform",
        "iPhone 14",
        "--version",
        "18.1",
    ],
)

targets.variant(
    name = "SIM_IPHONE_15_18_0",
    identifier = "iPhone 15 18.0",
    mixins = [
        "ios_runtime_cache_18_0",
    ],
    args = [
        "--platform",
        "iPhone 15",
        "--version",
        "18.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_PLUS_16_4",
    identifier = "iPhone 14 Plus 16.4",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPhone 14 Plus",
        "--version",
        "16.4",
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
    name = "SIM_IPHONE_14_PLUS_18_0",
    identifier = "iPhone 14 Plus 18.0",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_18_0",
    ],
    args = [
        "--platform",
        "iPhone 14 Plus",
        "--version",
        "18.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_PRO_MAX_17_5",
    identifier = "iPhone 14 Pro Max 17.5",
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
    name = "SIM_IPHONE_15_PRO_MAX_18_0",
    identifier = "iPhone 15 Pro Max 18.0",
    mixins = [
        "ios_runtime_cache_18_0",
    ],
    args = [
        "--platform",
        "iPhone 15 Pro Max",
        "--version",
        "18.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_SE_3RD_GEN_16_4",
    identifier = "iPhone SE (3rd generation) 16.4",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "16.4",
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
    name = "SIM_IPHONE_SE_3RD_GEN_18_0",
    identifier = "iPhone SE (3rd generation) 18.0",
    mixins = [
        "ios_runtime_cache_18_0",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "18.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_SE_3RD_GEN_18_1",
    identifier = "iPhone SE (3rd generation) 18.1",
    mixins = [
        "ios_runtime_cache_18_1",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "18.1",
    ],
)

targets.variant(
    name = "SIM_IPHONE_X_16_4",
    identifier = "iPhone X 16.4",
    generate_pyl_entry = False,
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPhone X",
        "--version",
        "16.4",
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

# This set of variants is encoded in a json file so that
# chrome/official.infra/lacros-skylab-tests-cros-img-roller can update the
# variant definitions
[targets.variant(
    name = name,
    enabled = variant.get("enabled"),
    identifier = variant["identifier"],
    # The cros_chrome_version field isn't used by the generator: it's used by
    # the cros skylab test image roller to compare against other data sources
    skylab = targets.skylab(**{
        k: v
        for k, v in variant["skylab"].items()
        if k != "cros_chrome_version"
    }),
) for name, variant in json.decode(io.read_file("./cros-skylab-variants.json")).items()]

targets.variant(
    name = "WIN10_INTEL_UHD_630_STABLE",
    identifier = "8086:9bc5",
    mixins = [
        "swarming_containment_auto",
        "win10_intel_uhd_630_stable",
    ],
)

targets.variant(
    name = "WIN10_NVIDIA_GTX_1660_STABLE",
    identifier = "10de:2184",
    mixins = [
        "win10_nvidia_gtx_1660_stable",
    ],
)
