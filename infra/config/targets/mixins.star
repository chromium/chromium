# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/targets.star", "targets")

targets.mixin(
    name = "10-x86-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android29.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android29",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android29",
                path = ".android_emulator/generic_android29",
            ),
        ],
    ),
)

targets.mixin(
    name = "11-x86-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android30.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android30",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android30",
                path = ".android_emulator/generic_android30",
            ),
        ],
    ),
)

targets.mixin(
    name = "12-x64-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android31.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android31",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android31",
                path = ".android_emulator/generic_android31",
            ),
        ],
    ),
)

targets.mixin(
    name = "12l-x64-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android32_foldable.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android32_foldable",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android32_foldable",
                path = ".android_emulator/generic_android32_foldable",
            ),
        ],
    ),
)

targets.mixin(
    name = "12l-x64-emulator-experimental",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android32_foldable_experimental.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android32_foldable_experimental",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android32_foldable_experimental",
                path = ".android_emulator/generic_android32_foldable_experimental",
            ),
        ],
    ),
)

targets.mixin(
    name = "13-x64-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android33.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android33",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android33",
                path = ".android_emulator/generic_android33",
            ),
        ],
    ),
)

targets.mixin(
    name = "13-x64-emulator-experimental",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android33_experimental.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android33_experimental",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android33_experimental",
                path = ".android_emulator/generic_android33_experimental",
            ),
        ],
    ),
)

targets.mixin(
    name = "android_r",
    swarming = targets.swarming(
        dimensions = {
            "device_os": "R",
        },
    ),
)

targets.mixin(
    name = "android_t",
    swarming = targets.swarming(
        dimensions = {
            "device_os": "TP1A.220624.021",
        },
    ),
)

targets.mixin(
    name = "android_user",
    swarming = targets.swarming(
        dimensions = {
            "device_os_type": "user",
        },
    ),
)

targets.mixin(
    name = "arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
        },
    ),
)

targets.mixin(
    name = "blink_tests_write_run_histories",
    args = [
        "--write-run-histories-to=${ISOLATED_OUTDIR}/run_histories.json",
    ],
)

targets.mixin(
    name = "bullhead",
    swarming = targets.swarming(
        dimensions = {
            "device_type": "bullhead",
            "os": "Android",
        },
    ),
)

targets.mixin(
    name = "chrome-finch-swarming-pool",
    swarming = targets.swarming(
        dimensions = {
            "pool": "chrome.tests.finch",
        },
    ),
)

targets.mixin(
    name = "chrome-swarming-pool",
    swarming = targets.swarming(
        dimensions = {
            "pool": "chrome.tests",
        },
    ),
)

targets.mixin(
    name = "chrome-tester-service-account",
    swarming = targets.swarming(
        service_account = "chrome-tester@chops-service-accounts.iam.gserviceaccount.com",
    ),
)

targets.mixin(
    name = "chromeos-amd64-generic",
    args = [
        "--magic-vm-cache=magic_cros_vm_cache",
    ],
    swarming = targets.swarming(
        dimension_sets = [
            {
                "cpu": "x86",
                "kvm": "1",
                "os": "Ubuntu-18.04",
                "pool": "chromium.tests",
            },
        ],
        optional_dimensions = {
            60: {
                "caches": "cros_vm",
            },
        },
        # This cache dir doesn't actually contain anything. Rather, its presence
        # merely signals to the fleet that the a CrOS VM test recently ran on
        # the bot and that its large VM image is likely still present in the
        # bot's isolated cache. So by optionally targeting bots with that magic
        # dir, CrOS VM tests can naturally have higher cache hit rates.
        named_caches = [
            swarming.cache(
                name = "cros_vm",
                path = "magic_cros_vm_cache",
            ),
        ],
    ),
)

targets.mixin(
    name = "chromeos-betty",
    args = [
        "--magic-vm-cache=magic_cros_vm_cache",
    ],
    swarming = targets.swarming(
        dimension_sets = [
            {
                "cpu": "x86",
                "kvm": "1",
                "gce": "1",
                "os": "Ubuntu-18.04",
                "pool": "chrome.tests",
            },
        ],
        optional_dimensions = {
            60: {
                "caches": "cros_vm",
            },
        },
        # See the 'chromeos-amd64-generic' mixin above for the purpose of this
        # cache.
        named_caches = [
            swarming.cache(
                name = "cros_vm",
                path = "magic_cros_vm_cache",
            ),
        ],
    ),
)

targets.mixin(
    name = "chromeos-betty-finch",
    args = [
        "--magic-vm-cache=magic_cros_vm_cache",
    ],
    swarming = targets.swarming(
        dimension_sets = [
            {
                "cpu": "x86",
                "kvm": "1",
                "gce": "1",
                "os": "Ubuntu-18.04",
                "pool": "chrome.tests.finch",
            },
        ],
        optional_dimensions = {
            60: {
                "caches": "cros_vm",
            },
        },
        # See the 'chromeos-amd64-generic' mixin above for the purpose of this
        # cache.
        named_caches = [
            swarming.cache(
                name = "cros_vm",
                path = "magic_cros_vm_cache",
            ),
        ],
    ),
)

targets.mixin(
    name = "chromeos-jacuzzi",
    swarming = targets.swarming(
        dimensions = {
            "os": "ChromeOS",
            "device_type": "jacuzzi",
        },
    ),
)

targets.mixin(
    name = "chromeos-kevin",
    swarming = targets.swarming(
        dimensions = {
            "os": "ChromeOS",
            "device_type": "kevin",
            "pool": "chromium.tests",
        },
    ),
)

targets.mixin(
    name = "chromeos-octopus",
    swarming = targets.swarming(
        dimensions = {
            "os": "ChromeOS",
            "device_type": "octopus",
        },
    ),
)

targets.mixin(
    name = "chromeos-reven",
    args = [
        "--magic-vm-cache=magic_cros_reven_vm_cache",
    ],
    swarming = targets.swarming(
        dimension_sets = [
            {
                "cpu": "x86",
                "kvm": "1",
                "gce": "1",
                "os": "Ubuntu-18.04",
                "pool": "chrome.tests",
            },
        ],
        optional_dimensions = {
            60: {
                "caches": "cros_reven_vm",
            },
        },
        # See the 'chromeos-amd64-generic' mixin above for the purpose of this
        # cache.
        named_caches = [
            swarming.cache(
                name = "cros_reven_vm",
                path = "magic_cros_reven_vm_cache",
            ),
        ],
    ),
)

targets.mixin(
    name = "chromium-tester-dev-service-account",
    swarming = targets.swarming(
        service_account = "chromium-tester-dev@chops-service-accounts.iam.gserviceaccount.com",
    ),
)

targets.mixin(
    name = "chromium-tester-service-account",
    swarming = targets.swarming(
        service_account = "chromium-tester@chops-service-accounts.iam.gserviceaccount.com",
    ),
)

# Used for invert CQ tests selection. Adding ci_only: False to
# test_suite_exceptions.pyl to select tests that are allowed on CQ builders.
targets.mixin(
    name = "ci_only",
    ci_only = True,
)

targets.mixin(
    name = "dawn_end2end_gpu_test",
    args = [
        "--use-gpu-in-tests",
        # Dawn test retries deliberately disabled to prevent flakiness.
        "--test-launcher-retry-limit=0",
        "--exclusive-device-type-preference=discrete,integrated",
    ],
)

targets.mixin(
    name = "disable_check_flakiness_web_tests",
    check_flakiness_for_new_tests = False,
)

targets.mixin(
    name = "disable_field_trial_config_for_earl_grey",
    args = [
        "--extra-app-args=--disable-field-trial-config",
    ],
)

targets.mixin(
    name = "docker",
    swarming = targets.swarming(
        dimensions = {
            "inside_docker": "1",
        },
    ),
)

targets.mixin(
    name = "emulator-4-cores",
    swarming = targets.swarming(
        dimensions = {
            "device_os": None,
            "device_type": None,
            "pool": "chromium.tests.avd",
            "cores": "4",
        },
    ),
)

targets.mixin(
    name = "emulator-8-cores",
    swarming = targets.swarming(
        dimensions = {
            "device_os": None,
            "device_type": None,
            "pool": "chromium.tests.avd",
            "cores": "8",
        },
    ),
)

targets.mixin(
    name = "finch-chromium-swarming-pool",
    swarming = targets.swarming(
        dimensions = {
            "pool": "chromium.tests.finch",
        },
    ),
)

# Pixel 4
targets.mixin(
    name = "flame",
    swarming = targets.swarming(
        dimensions = {
            "device_type": "flame",
            "os": "Android",
        },
    ),
)

targets.mixin(
    name = "fuchsia-code-coverage",
    args = [
        "--code-coverage-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.mixin(
    name = "fuchsia-persistent-emulator",
    args = [
        "--everlasting",
    ],
)

targets.mixin(
    name = "fuchsia_logs",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}/logs",
    ],
)

targets.mixin(
    name = "gpu-swarming-pool",
    swarming = targets.swarming(
        dimensions = {
            "pool": "chromium.tests.gpu",
        },
    ),
)

# Use of this mixin signals to the recipe that the test uploads its results
# to result-sink and doesn't need to be wrapped by result_adapter.
targets.mixin(
    name = "has_native_resultdb_integration",
    resultdb = targets.resultdb(
        enable = True,
        # TODO(crbug.com/1163797): Remove the 'enable' field in favor of
        # 'has_native_resultdb_integration'.
        has_native_resultdb_integration = True,
    ),
)

targets.mixin(
    name = "integrity_high",
    swarming = targets.swarming(
        dimensions = {
            "integrity": "high",
        },
    ),
)

targets.mixin(
    name = "ios_custom_webkit",
    args = [
        "--args-json",
        "{\"test_args\": [\"--run-with-custom-webkit\"]}",
    ],
)

targets.mixin(
    name = "ios_output_disabled_tests",
    args = [
        "--output-disabled-tests",
    ],
)

targets.mixin(
    name = "ios_restart_device",
    args = [
        "--restart",
    ],
)

targets.mixin(
    name = "ios_runtime_cache_15_5",
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "runtime_ios_15_5",
                path = "Runtime-ios-15.5",
            ),
        ],
    ),
)

targets.mixin(
    name = "ios_runtime_cache_16_2",
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "runtime_ios_16_2",
                path = "Runtime-ios-16.2",
            ),
        ],
    ),
)

targets.mixin(
    name = "ios_runtime_cache_16_4",
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "runtime_ios_16_4",
                path = "Runtime-ios-16.4",
            ),
        ],
    ),
)

targets.mixin(
    name = "isolate_profile_data",
    isolate_profile_data = True,
)

targets.mixin(
    name = "junit-swarming",
    swarming = targets.swarming(
        dimensions = {
            "cores": "8",
            "pool": "chromium.tests",
        },
    ),
)

targets.mixin(
    name = "kitkat-x86-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android19.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android19",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android19",
                path = ".android_emulator/generic_android19",
            ),
        ],
    ),
)

targets.mixin(
    name = "limited_capacity_bot",
    # Some FYI bot configurations have a limited number of bots in the swarming
    # pool. Increase the default expiration_sec time from 1 hour to 6 hours to
    # prevent shards from timing out.
    swarming = targets.swarming(
        expiration_sec = 21600,
    ),
)

targets.mixin(
    name = "linux-archive-rel-args",
    args = [
        "linux-release-64/sizes",
    ],
)

targets.mixin(
    name = "linux-bionic",
    swarming = targets.swarming(
        dimensions = {
            "os": "Ubuntu-18.04",
        },
    ),
)

targets.mixin(
    name = "linux-focal",
    swarming = targets.swarming(
        dimensions = {
            "os": "Ubuntu-20.04",
        },
    ),
)

targets.mixin(
    name = "linux-jammy",
    swarming = targets.swarming(
        dimensions = {
            "os": "Ubuntu-22.04",
        },
    ),
)

targets.mixin(
    name = "linux-jammy-or-bionic",
    swarming = targets.swarming(
        dimensions = {
            "os": "Ubuntu-22.04|Ubuntu-18.04",
        },
    ),
)

# TODO(crbug.com/1260217): Remove the xenial mixin once the MSAN bots have
# migrated to focal.
targets.mixin(
    name = "linux-xenial",
    swarming = targets.swarming(
        dimensions = {
            "os": "Ubuntu-16.04",
        },
    ),
)

targets.mixin(
    name = "linux_amd_rx_5500_xt",
    swarming = targets.swarming(
        dimensions = {
            "gpu": "1002:7340",
            "os": "Ubuntu-18.04.6",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "linux_intel_uhd_630_experimental",
    swarming = targets.swarming(
        dimensions = {
            "gpu": "8086:9bc5-20.0.8",
            "os": "Ubuntu-18.04.6",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "linux_intel_uhd_630_stable",
    swarming = targets.swarming(
        dimensions = {
            "gpu": "8086:9bc5-20.0.8",
            "os": "Ubuntu-18.04.6",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "linux_nvidia_gtx_1660_experimental",
    swarming = targets.swarming(
        dimensions = {
            "gpu": "10de:2184-440.100",
            "os": "Ubuntu-18.04.5|Ubuntu-18.04.6",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "linux_nvidia_gtx_1660_stable",
    # TODO(crbug.com/1408314): The swarming dimensions for
    # webgpu_blink_web_tests and webgpu_cts_tests on linux-code-coverage
    # must be kept manually in sync with the appropriate mixin; currently,
    # this one, which is used by Dawn Linux x64 Release (NVIDIA).
    swarming = targets.swarming(
        dimensions = {
            "gpu": "10de:2184-440.100",
            "os": "Ubuntu-18.04.5|Ubuntu-18.04.6",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "lollipop-x86-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android22.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android22",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android22",
                path = ".android_emulator/generic_android22",
            ),
        ],
    ),
)

targets.mixin(
    name = "long_skylab_timeout",
    timeout_sec = 10800,
)

targets.mixin(
    name = "mac-archive-rel-args",
    args = [
        "mac-release/sizes",
    ],
)

targets.mixin(
    name = "mac_10.13",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-10.13.6",
        },
    ),
)

targets.mixin(
    name = "mac_10.14",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-10.14.6",
        },
    ),
)

targets.mixin(
    name = "mac_10.15",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-10.15",
        },
    ),
)

targets.mixin(
    name = "mac_11_arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "os": "Mac-11",
        },
    ),
)

targets.mixin(
    name = "mac_11_x64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-11|Mac-10.16",
        },
    ),
)

targets.mixin(
    name = "mac_12_arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "os": "Mac-12",
        },
    ),
)

targets.mixin(
    name = "mac_12_or_13_arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "os": "Mac-12|Mac-13",
        },
    ),
)

targets.mixin(
    name = "mac_12_or_13_t2_x64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "mac_model": "Macmini8,1",
            "os": "Mac-12|Mac-13",
        },
    ),
)

targets.mixin(
    name = "mac_12_or_13_x64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-12|Mac-13",
        },
    ),
)

targets.mixin(
    name = "mac_12_x64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-12",
        },
    ),
)

targets.mixin(
    name = "mac_13_arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "os": "Mac-13",
        },
    ),
)

targets.mixin(
    name = "mac_13_x64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-13",
        },
    ),
)

targets.mixin(
    name = "mac_arm64_apple_m1_gpu_experimental",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "mac_model": "Macmini9,1",
            "os": "Mac-13.2",
            "pool": "chromium.tests",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_arm64_apple_m1_gpu_stable",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "mac_model": "Macmini9,1",
            "os": "Mac-12.5",
            "pool": "chromium.tests",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_arm64_apple_m2_retina_gpu_stable",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "mac_model": "Mac14,7",
            "os": "Mac-13.3.1",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
            "hidpi": "1",
        },
    ),
)

targets.mixin(
    name = "mac_beta_arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "os": "Mac-13",
        },
    ),
)

targets.mixin(
    name = "mac_beta_x64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-13",
        },
    ),
)

targets.mixin(
    name = "mac_mini_intel_gpu_experimental",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "8086:3e9b",
            "os": "Mac-13.3.1",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_mini_intel_gpu_stable",
    # TODO(crbug.com/1408314): The swarming dimensions for
    # webgpu_blink_web_tests and webgpu_cts_tests on mac-code-coverage
    # must be kept manually in sync with the appropriate mixin; currently,
    # this one, which is used by Dawn Mac x64 Release (Intel).
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "8086:3e9b",
            "os": "Mac-13.3.1",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_pro_amd_gpu",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "1002:679e",
            "os": "Mac-12.4",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_retina_amd_gpu_experimental",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "1002:67ef",
            "hidpi": "1",
            "os": "Mac-13.2.1",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_retina_amd_gpu_stable",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "1002:67ef",
            "hidpi": "1",
            "os": "Mac-13.2.1",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_retina_nvidia_gpu_experimental",
    # Currently the same as the stable version.
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "10de:0fe9",
            "hidpi": "1",
            "os": "Mac-10.14.6",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_retina_nvidia_gpu_stable",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "10de:0fe9",
            "hidpi": "1",
            "os": "Mac-10.14.6",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_toolchain",
    swarming = targets.swarming(
        cipd_packages = [
            targets.cipd_package(
                package = "infra/tools/mac_toolchain/${platform}",
                location = ".",
                revision = "git_revision:3e597065cb23c1fe03aeb2ebd792d83e0709c5c2",
            ),
        ],
    ),
)

# mac_x64 is used as a prefered OS dimension for mac platform instead of any
# mac OS version. It selects the most representative dimension on Swarming.
targets.mixin(
    name = "mac_x64",
    swarming = targets.swarming(
        dimension_sets = [
            {
                "os": "Mac-12",
                "cpu": "x86-64",
            },
        ],
    ),
)

targets.mixin(
    name = "marshmallow",
    swarming = targets.swarming(
        dimensions = {
            "device_os": "MMB29Q",
        },
    ),
)

targets.mixin(
    name = "marshmallow-x86-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android23.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android23",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android23",
                path = ".android_emulator/generic_android23",
            ),
        ],
    ),
)

# NVIDIA Shield TV 2019
targets.mixin(
    name = "mdarcy",
    swarming = targets.swarming(
        dimensions = {
            "device_type": "mdarcy",
            "os": "Android",
        },
    ),
)

targets.mixin(
    name = "no_gpu",
    swarming = targets.swarming(
        dimensions = {
            "gpu": "none",
        },
    ),
)

targets.mixin(
    name = "nougat",
    swarming = targets.swarming(
        dimensions = {
            "device_os": "N2G48C",
        },
    ),
)

targets.mixin(
    name = "nougat-x86-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android24.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android24",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android24",
                path = ".android_emulator/generic_android24",
            ),
        ],
    ),
)

targets.mixin(
    name = "oreo-x86-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android27.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android27",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android27",
                path = ".android_emulator/generic_android27",
            ),
        ],
    ),
)

targets.mixin(
    name = "oreo_fleet",
    swarming = targets.swarming(
        dimensions = {
            "device_os": "OPM4.171019.021.P2",
            "device_os_flavor": "google",
        },
    ),
)

# Pixel 6
targets.mixin(
    name = "oriole",
    swarming = targets.swarming(
        dimensions = {
            "device_type": "oriole",
            "os": "Android",
        },
    ),
)

targets.mixin(
    name = "out_dir_arg",
    args = [
        "--out-dir",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.mixin(
    name = "pie-x86-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/generic_android28.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android28",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android28",
                path = ".android_emulator/generic_android28",
            ),
        ],
    ),
)

targets.mixin(
    name = "pie_fleet",
    swarming = targets.swarming(
        dimensions = {
            "device_os": "PQ3A.190801.002",
            "device_os_flavor": "google",
        },
    ),
)

targets.mixin(
    name = "pie_generic",
    swarming = targets.swarming(
        dimensions = {
            "device_os": "P",
        },
    ),
)

targets.mixin(
    name = "record_failed_tests",
    args = [
        "--record-video",
        "failed_only",
    ],
)

targets.mixin(
    name = "s_generic",
    swarming = targets.swarming(
        dimensions = {
            "device_os": "S",
        },
    ),
)

targets.mixin(
    name = "samsung_a13",
    swarming = targets.swarming(
        dimensions = {
            "device_type": "a13",
            "os": "Android",
        },
    ),
)

targets.mixin(
    name = "samsung_a23",
    swarming = targets.swarming(
        dimensions = {
            "device_type": "a23",
            "os": "Android",
        },
    ),
)

targets.mixin(
    name = "skia_gold_test",
    args = [
        "--git-revision=${got_revision}",
        # BREAK GLASS IN CASE OF EMERGENCY
        # Uncommenting this argument will bypass all interactions with Skia
        # Gold in any tests that use it. This is meant as a temporary
        # emergency stop in case of a Gold outage that's affecting the bots.
        # "--bypass-skia-gold-functionality",
    ],
    precommit_args = [
        "--gerrit-issue=${patch_issue}",
        "--gerrit-patchset=${patch_set}",
        "--buildbucket-id=${buildbucket_build_id}",
    ],
)

targets.mixin(
    name = "swarming_containment_auto",
    swarming = targets.swarming(
        containment_type = "AUTO",
    ),
)

targets.mixin(
    name = "timeout_15m",
    swarming = targets.swarming(
        hard_timeout_sec = 900,
        io_timeout_sec = 900,
    ),
)

targets.mixin(
    name = "updater-default-pool",
    swarming = targets.swarming(
        dimensions = {
            "pool": "chromium.tests",
        },
    ),
)

targets.mixin(
    name = "updater-mac-pool",
    swarming = targets.swarming(
        dimensions = {
            # Bots in this pool support passwordless sudo.
            "pool": "chromium.updater.mac",
        },
    ),
)

targets.mixin(
    name = "updater-win-uac-pool",
    swarming = targets.swarming(
        dimensions = {
            "pool": "chromium.win.uac",
        },
    ),
)

targets.mixin(
    name = "vr_instrumentation_test",
    args = [
        "--remove-system-package=com.google.vr.vrcore",
        "--additional-apk=//third_party/gvr-android-sdk/test-apks/vr_services/vr_services_current.apk",
    ],
)

# Pixel 2
targets.mixin(
    name = "walleye",
    swarming = targets.swarming(
        dimensions = {
            "device_type": "walleye",
            "os": "Android",
        },
    ),
)

targets.mixin(
    name = "webgpu_cts",
    args = [
        # crbug.com/953991 Ensure WebGPU is ready before running tests
        "--initialize-webgpu-adapter-at-startup-timeout-ms=60000",
    ],
    linux_args = [
        "--no-xvfb",
        "--additional-driver-flag=--enable-features=Vulkan",
    ],
    mac_args = [
        "--platform=mac-mac11",
    ],
    win64_args = [
        "--target=Release_x64",
    ],
    merge = targets.merge(
        script = "//third_party/blink/tools/merge_web_test_results.py",
        args = [
            "--verbose",
        ],
    ),
    resultdb = targets.resultdb(
        enable = True,
    ),
)

targets.mixin(
    name = "webgpu_telemetry_cts",
    args = [
        "--extra-browser-args=--force_high_performance_gpu",
        "--use-webgpu-power-preference=default-high-performance",
        "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
    ],
    linux_args = [
        "--extra-browser-args=--enable-features=Vulkan",
    ],
)

targets.mixin(
    name = "win10",
    swarming = targets.swarming(
        dimensions = {
            "os": "Windows-10-19045",
        },
    ),
)

targets.mixin(
    name = "win10-any",
    swarming = targets.swarming(
        dimensions = {
            "os": "Windows-10",
        },
    ),
)

targets.mixin(
    name = "win10_amd_rx_5500_xt",
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            "gpu": "1002:7340",
            "os": "Windows-10",
            "pool": "chromium.tests.gpu.experimental",
        },
    ),
)

targets.mixin(
    name = "win10_gce_gpu_pool",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "none",
            "os": "Windows-10",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "win10_intel_uhd_630_experimental",
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            "gpu": "8086:9bc5-31.0.101.2111",
            "os": "Windows-10",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "win10_intel_uhd_630_stable",
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            "gpu": "8086:9bc5-31.0.101.2111",
            "os": "Windows-10",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "win10_intel_uhd_630_stable_dimension_set",
    # We use explicit 'dimension_sets' instead of 'dimensions' since this is
    # used in conjunction with 'win10_nvidia_gtx_1660_stable_dimension_set'
    # to trigger tests on multiple configurations.
    swarming = targets.swarming(
        dimension_sets = [
            {
                "display_attached": "1",
                "gpu": "8086:9bc5-31.0.101.2111",
                "os": "Windows-10",
                "pool": "chromium.tests.gpu",
            },
        ],
    ),
)

targets.mixin(
    name = "win10_nvidia_gtx_1660_experimental",
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            "gpu": "10de:2184-27.21.14.5638",
            "os": "Windows-10-18363",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "win10_nvidia_gtx_1660_stable",
    # TODO(crbug.com/1408314): The swarming dimensions for
    # webgpu_blink_web_tests and webgpu_cts_tests on win10-code-coverage
    # must be kept manually in sync with the appropriate mixin; currently,
    # this one, which is used by Dawn Win10 x64 Release (NVIDIA).
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            "gpu": "10de:2184-27.21.14.5638",
            "os": "Windows-10-18363",
            "pool": "chromium.tests.gpu",
        },
    ),
)

# Version of win10_nvidia_gtx_1660_stable that uses 'dimension_sets'
# instead of 'dimensions' so it can be used to trigger tests on multiple
# configurations.
targets.mixin(
    name = "win10_nvidia_gtx_1660_stable_dimension_set",
    swarming = targets.swarming(
        dimension_sets = [
            {
                "display_attached": "1",
                "gpu": "10de:2184-27.21.14.5638",
                "os": "Windows-10-18363",
                "pool": "chromium.tests.gpu",
            },
        ],
    ),
)

targets.mixin(
    name = "win11",
    swarming = targets.swarming(
        dimensions = {
            "os": "Windows-11-22000",
        },
    ),
)

targets.mixin(
    name = "win_arm64",
    swarming = targets.swarming(
        dimensions = {
            # TODO(https://crbug.com/1422826): Update the right cpu value.
            "cpu": None,
            "os": "Windows-11",
            "pool": "chrome.tests.arm64",
        },
        # The resources are limited in the pool.
        # The slowest test is expected to run >9 hours.
        expiration_sec = 64800,  # 18 hours
        hard_timeout_sec = 43200,  # 12 hours
    ),
)

targets.mixin(
    name = "x86-64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
        },
    ),
)

targets.mixin(
    name = "xcode_14_beta",
    args = [
        "--xcode-build-version",
        "14e222b",
    ],
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "xcode_ios_14e222b",
                path = "Xcode.app",
            ),
        ],
    ),
)

# Xcode 14 on iOS main.
targets.mixin(
    name = "xcode_14_main",
    args = [
        "--xcode-build-version",
        "14c18",
    ],
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "xcode_ios_14c18",
                path = "Xcode.app",
            ),
        ],
    ),
)

targets.mixin(
    name = "xcode_14_readline_timeout",
    args = [
        "--readline-timeout",
        "600",
    ],
)

targets.mixin(
    name = "xcode_parallelization",
    args = [
        "--xcode-parallelization",
    ],
)

targets.mixin(
    name = "xctest",
    args = [
        "--xctest",
    ],
)
