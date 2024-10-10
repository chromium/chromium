# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/targets.star", "targets")

targets.mixin(
    name = "10-x86-emulator",
    generate_pyl_entry = False,
    args = [
        "--avd-config=../../tools/android/avd/proto/android_29_google_apis_x86.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_29_google_apis_x86",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_29_google_apis_x86",
                path = ".android_emulator/android_29_google_apis_x86",
            ),
        ],
    ),
)

targets.mixin(
    name = "11-x86-emulator",
    args = [
        "--avd-config=../../tools/android/avd/proto/android_30_google_apis_x86.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_30_google_apis_x86",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_30_google_apis_x86",
                path = ".android_emulator/android_30_google_apis_x86",
            ),
        ],
    ),
)

targets.mixin(
    name = "12-google-atd-x64-emulator",
    generate_pyl_entry = False,
    args = [
        "--avd-config=../../tools/android/avd/proto/android_31_google_atd_x64.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_31_google_atd_x64",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_31_google_atd_x64",
                path = ".android_emulator/android_31_google_atd_x64",
            ),
        ],
    ),
)

targets.mixin(
    name = "12-x64-emulator",
    description = "Run with android_31_google_apis_x64",
    args = [
        "--avd-config=../../tools/android/avd/proto/android_31_google_apis_x64.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_31_google_apis_x64",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_31_google_apis_x64",
                path = ".android_emulator/android_31_google_apis_x64",
            ),
        ],
    ),
)

# TODO(crbug.com/347759127): Re-enable after 12L FYI evaluation is done.
# targets.mixin(
#     name = "12l-google-atd-x64-emulator",
#     args = [
#         "--avd-config=../../tools/android/avd/proto/android_32_google_atd_x64_foldable.textpb",
#     ],
#     swarming = targets.swarming(
#         # soft affinity so that bots with caches will be picked first
#         optional_dimensions = {
#             60: {
#                 "caches": "android_32_google_atd_x64_foldable",
#             },
#         },
#         named_caches = [
#             swarming.cache(
#                 name = "android_32_google_atd_x64_foldable",
#                 path = ".android_emulator/android_32_google_atd_x64_foldable",
#             ),
#         ],
#     ),
# )

targets.mixin(
    name = "12l-fyi-x64-emulator",
    generate_pyl_entry = False,
    args = [
        "--avd-config=../../tools/android/avd/proto/android_32_google_apis_x64_foldable_fyi.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_32_google_apis_x64_foldable_fyi",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_32_google_apis_x64_foldable_fyi",
                path = ".android_emulator/android_32_google_apis_x64_foldable_fyi",
            ),
        ],
    ),
)

targets.mixin(
    name = "12l-x64-emulator",
    generate_pyl_entry = False,
    args = [
        "--avd-config=../../tools/android/avd/proto/android_32_google_apis_x64_foldable.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_32_google_apis_x64_foldable",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_32_google_apis_x64_foldable",
                path = ".android_emulator/android_32_google_apis_x64_foldable",
            ),
        ],
    ),
)

targets.mixin(
    name = "12l-landscape-x64-emulator",
    generate_pyl_entry = False,
    args = [
        "--avd-config=../../tools/android/avd/proto/android_32_google_apis_x64_foldable_landscape.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_32_google_apis_x64_foldable_landscape",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_32_google_apis_x64_foldable_landscape",
                path = ".android_emulator/android_32_google_apis_x64_foldable_landscape",
            ),
        ],
    ),
)

targets.mixin(
    name = "13-google-atd-x64-emulator",
    generate_pyl_entry = False,
    args = [
        "--avd-config=../../tools/android/avd/proto/android_33_google_atd_x64.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_33_google_atd_x64",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_33_google_atd_x64",
                path = ".android_emulator/android_33_google_atd_x64",
            ),
        ],
    ),
)

# TODO(crbug.com/370084605): Remove this mixin after the migration is done.
targets.mixin(
    name = "13-swangle-x64-emulator",
    generate_pyl_entry = False,
    description = "Run with android_33_google_apis_x64_swangle",
    args = [
        "--avd-config=../../tools/android/avd/proto/android_33_google_apis_x64_swangle.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_33_google_apis_x64_swangle",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_33_google_apis_x64_swangle",
                path = ".android_emulator/android_33_google_apis_x64_swangle",
            ),
        ],
    ),
)

targets.mixin(
    name = "13-x64-emulator",
    generate_pyl_entry = False,
    description = "Run with android_33_google_apis_x64",
    args = [
        "--avd-config=../../tools/android/avd/proto/android_33_google_apis_x64.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_33_google_apis_x64",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_33_google_apis_x64",
                path = ".android_emulator/android_33_google_apis_x64",
            ),
        ],
    ),
)

# TODO(crbug.com/370084605): Remove this mixin after the migration is done.
targets.mixin(
    name = "14-swangle-x64-emulator",
    generate_pyl_entry = False,
    description = "Run with android_34_google_apis_x64_swangle",
    args = [
        "--avd-config=../../tools/android/avd/proto/android_34_google_apis_x64_swangle.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_34_google_apis_x64_swangle",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_34_google_apis_x64_swangle",
                path = ".android_emulator/android_34_google_apis_x64_swangle",
            ),
        ],
    ),
)

targets.mixin(
    name = "14-x64-emulator",
    description = "Run with android_34_google_apis_x64",
    args = [
        "--avd-config=../../tools/android/avd/proto/android_34_google_apis_x64.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_34_google_apis_x64",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_34_google_apis_x64",
                path = ".android_emulator/android_34_google_apis_x64",
            ),
        ],
    ),
)

targets.mixin(
    name = "14-desktop-x64-emulator",
    generate_pyl_entry = False,
    description = "Run with android_34_desktop_x64",
    args = [
        "--avd-config=../../tools/android/avd/proto/android_34_desktop_x64.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_34_desktop_x64",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_34_desktop_x64",
                path = ".android_emulator/android_34_desktop_x64",
            ),
        ],
    ),
)

targets.mixin(
    name = "15-x64-emulator",
    generate_pyl_entry = False,
    description = "Run with android_35_google_apis_x64",
    args = [
        "--avd-config=../../tools/android/avd/proto/android_35_google_apis_x64.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_35_google_apis_x64",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_35_google_apis_x64",
                path = ".android_emulator/android_35_google_apis_x64",
            ),
        ],
    ),
)

# TODO(crbug.com/370084605): Remove this mixin after the migration is done.
targets.mixin(
    name = "15-swangle-x64-emulator",
    generate_pyl_entry = False,
    description = "Run with android_35_google_apis_x64_swangle",
    args = [
        "--avd-config=../../tools/android/avd/proto/android_35_google_apis_x64_swangle.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_35_google_apis_x64_swangle",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_35_google_apis_x64_swangle",
                path = ".android_emulator/android_35_google_apis_x64_swangle",
            ),
        ],
    ),
)

targets.mixin(
    name = "amd_radeon_rx_5500_xt",
    swarming = targets.swarming(
        dimensions = {
            "gpu": "1002:7340",
        },
    ),
)

targets.mixin(
    name = "android",
    swarming = targets.swarming(
        dimensions = {
            "os": "Android",
            "cpu": None,
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
    name = "chrome-flame-fleet-pool",
    swarming = targets.swarming(
        dimensions = {
            "device_type": "flame",
            "device_os": "R",
            "pool": "chrome.tests",
            "os": "Android",
        },
    ),
)

targets.mixin(
    name = "chrome-intelligence-swarming-pool",
    swarming = targets.swarming(
        dimensions = {
            "pool": "chrome.tests.intelligence",
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
    name = "crosier-no-arc",
    args = [
        # All is_chrome_branded Chrome has components/arc, but reven board
        # don't have ChromeOS daemons and resources necessary for ARC.
        # Disable ARC crosier tests on reven.
        "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.reven.chromeos_integration_tests.filter",
    ],
)

targets.mixin(
    name = "shards-20",
    shards = 20,
)

targets.mixin(
    name = "shards-30",
    shards = 30,
)

targets.mixin(
    name = "shards-50",
    shards = 50,
)

targets.mixin(
    name = "chromeos-generic-vm",
    args = [
        "--magic-vm-cache=magic_cros_vm_cache",
    ],
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "kvm": "1",
            "os": "Ubuntu-22.04",
            "pool": "chromium.tests",
        },
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
    name = "chromeos-betty-finch",
    args = [
        "--board=betty",
        "--magic-vm-cache=magic_cros_vm_cache",
    ],
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "kvm": "1",
            "gce": "1",
            "os": "Ubuntu-22.04",
            "pool": "chrome.tests.finch",
        },
        optional_dimensions = {
            60: {
                "caches": "cros_vm",
            },
        },
        # See the 'chromeos-generic-vm' mixin above for the purpose of this
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
    name = "chromeos-tast-public-builder",
    skylab = targets.skylab(
        args = [
            # FieldTrial is disabled on ChromeOS builders but not in this builder.
            # Notify Tast to handle the different UI by that.
            "tast.setup.FieldTrialConfig=enable",

            # Tests using the default gaia pool cannot be run by public builders.
            # These variables are fed by private bundles, thus not for public builders.
            "maybemissingvars=ui\\.(gaiaPoolDefault|signinProfileTestExtensionManifestKey)|uidetection\\.(key|key_type|server)",

            # Use "hash" method to shrding of test tests. This should balance the
            # execution time among shards in a better way.
            "shard_method=hash",
        ],
    ),
)

targets.mixin(
    name = "chromium_nexus_5x_oreo",
    swarming = targets.swarming(
        dimensions = {
            "device_os": "OPR4.170623.020",
            "device_os_flavor": "google",
            "device_type": "bullhead",
            "os": "Android",
            "pool": "chromium.tests",
        },
    ),
)

targets.mixin(
    name = "chromium_pixel_2_pie",
    swarming = targets.swarming(
        dimensions = {
            "device_os": "PQ3A.190801.002",
            "device_os_flavor": "google",
            "device_type": "walleye",
            "os": "Android",
            "pool": "chromium.tests",
        },
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
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        service_account = "chromium-tester@chops-service-accounts.iam.gserviceaccount.com",
    ),
)

# Used for invert CQ tests selection. Adding ci_only: False to
# test_suite_exceptions.pyl to select tests that are allowed on CQ builders.
targets.mixin(
    name = "ci_only",
    generate_pyl_entry = False,
    ci_only = True,
)

targets.mixin(
    name = "experiments",
    experiment_percentage = 100,
)

targets.mixin(
    name = "chromium-tests-oslogin",
    swarming = targets.swarming(
        dimensions = {
            "pool": "chromium.tests.oslogin",
        },
    ),
)

targets.mixin(
    name = "dawn_end2end_gpu_test",
    args = [
        "--use-gpu-in-tests",
        "--exclusive-device-type-preference=discrete,integrated",
        # Dawn test retries deliberately disabled to prevent flakiness.
        "--test-launcher-retry-limit=0",
        # Reduces size of stdout of a batch crbug.com/1456415
        "--test-launcher-batch-limit=512",
    ],
)

targets.mixin(
    name = "disable_field_trial_config_for_earl_grey",
    args = [
        "--extra-app-args=--disable-field-trial-config",
    ],
)

targets.mixin(
    name = "docker",
    generate_pyl_entry = False,
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
    name = "emulator-enable-network",
    args = [
        "--emulator-enable-network",
    ],
    swarming = targets.swarming(
        idempotent = False,  # Set to False because network is enabled.
    ),
)

# In //testing/buildbot, what test type a test is expanded as depends on the
# test_suites key that the builder puts it under, so gtests included under the
# isolated_scripts suite type would get expanded as isolated scripts. In
# starlark, tests know what type they are and their type determines how they are
# expanded, which allows tests of different types to in the same bundle. This
# mixin enables using gtests as isolated script tests.
targets.mixin(
    name = "expand-as-isolated-script",
    generate_pyl_entry = False,
    expand_as_isolated_script = True,
)

targets.mixin(
    name = "finch-chromium-swarming-pool",
    swarming = targets.swarming(
        dimensions = {
            "pool": "chromium.tests.finch",
        },
    ),
)

targets.mixin(
    name = "fuchsia-code-coverage",
    args = [
        "--code-coverage-dir=${ISOLATED_OUTDIR}",
    ],
)

# TODO(b/300509814): Large device spec should be the default choice.
# Choose virtual_device_large spec for more ram. This mixin works on emulators
# only.
targets.mixin(
    name = "fuchsia-large-device-spec",
    args = [
        "--device-spec=virtual_device_large",
    ],
)

targets.mixin(
    name = "fuchsia-persistent-emulator",
    generate_pyl_entry = False,
    args = [
        "--everlasting",
    ],
    swarming = targets.swarming(
        # The persistent emulator will only be used on dedicated fuchsia pool so
        # that there isn't a need of cache affinity.
        named_caches = [
            swarming.cache(
                name = "fuchsia_emulator_cache",
                path = ".fuchsia_emulator/fuchsia-everlasting-emulator",
            ),
        ],
    ),
)

targets.mixin(
    name = "upload_inv_extended_properties",
    generate_pyl_entry = False,
    resultdb = targets.resultdb(
        enable = True,
        inv_extended_properties_dir = "${ISOLATED_OUTDIR}/invocations",
    ),
)

targets.mixin(
    name = "gce",
    swarming = targets.swarming(
        dimensions = {
            "gce": "1",
        },
    ),
)

targets.mixin(
    name = "gpu_integration_test_common_args",
    args = [
        targets.magic_args.GPU_PARALLEL_JOBS,
    ],
    android_args = [
        targets.magic_args.GPU_TELEMETRY_NO_ROOT_FOR_UNROOTED_DEVICES,
        # See crbug.com/333414298 for context on why this is necessary.
        "--initial-find-device-attempts=3",
    ],
    chromeos_args = [
        targets.magic_args.CROS_TELEMETRY_REMOTE,
    ],
    # TODO(crbug.com/40862371): having --xvfb and --no-xvfb is confusing.
    lacros_args = [
        "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
        "--xvfb",
        "--no-xvfb",
        "--use-weston",
        "--weston-use-gl",
    ],
)

targets.mixin(
    name = "gpu_nvidia_shield_tv_stable",
    swarming = targets.swarming(
        dimensions = {
            "os": "Android",
            "device_type": "mdarcy",
            "device_os": "PPR1.180610.011",
            "device_os_type": "userdebug",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "gpu_pixel_4_stable",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "os": "Android",
            "device_type": "flame",
            "device_os": "RP1A.201105.002",
            "device_os_type": "userdebug",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "gpu_pixel_6_experimental",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "os": "Android",
            "device_type": "oriole",
            "device_os": "AP1A.240405.002",
            "device_os_type": "userdebug",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "gpu_pixel_6_stable",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "os": "Android",
            "device_type": "oriole",
            "device_os": "TP1A.220624.021",
            "device_os_type": "userdebug",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "gpu_samsung_a13_stable",
    swarming = targets.swarming(
        dimensions = {
            "os": "Android",
            "device_type": "a13",
            "device_os": "S",
            "device_os_type": "user",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "gpu_samsung_a23_stable",
    swarming = targets.swarming(
        dimensions = {
            "os": "Android",
            "device_type": "a23",
            "device_os": "SP1A.210812.016",
            "device_os_type": "user",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "gpu_samsung_s23_stable",
    swarming = targets.swarming(
        dimensions = {
            # Unfortunately, "s23" is not exposed as a dimension. "dm1q" appears
            # to refer to the S23 specifically, while "kalama" is for the entire
            # S23 family.
            "device_type": "dm1q",
            "device_os": "UP1A.231005.007",
            "device_os_type": "user",
            "os": "Android",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "gpu_samsung_s24_stable",
    swarming = targets.swarming(
        dimensions = {
            # Unfortunately, "s24" is not exposed as a dimension. "e2s" appears
            # to refer to the S24 specifically, while "s5e9945" is for the
            # entire S24 family.
            "device_type": "e2s",
            "device_os": "UP1A.231005.007",
            "device_os_type": "user",
            "os": "Android",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "gpu-swarming-pool",
    generate_pyl_entry = targets.IGNORE_UNUSED,
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
        # TODO(crbug.com/40740370): Remove the 'enable' field in favor of
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
    name = "intel_uhd_630_or_770",
    swarming = targets.swarming(
        dimensions = {
            "gpu": "8086:9bc5|8086:4680",
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
    name = "ios_parallel_simulators",
    args = [
        "--clones",
        "2",
    ],
)

targets.mixin(
    name = "ios_restart_device",
    generate_pyl_entry = False,
    args = [
        "--restart",
    ],
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
    name = "ios_runtime_cache_17_5",
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "runtime_ios_17_5",
                path = "Runtime-ios-17.5",
            ),
        ],
    ),
)

targets.mixin(
    name = "ios_runtime_cache_18_0",
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "runtime_ios_18_0",
                path = "Runtime-ios-18.0",
            ),
        ],
    ),
)

targets.mixin(
    name = "ios_runtime_cache_18_1",
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "runtime_ios_18_1",
                path = "Runtime-ios-18.1",
            ),
        ],
    ),
)

targets.mixin(
    name = "ioswpt-chromium-swarming-pool",
    swarming = targets.swarming(
        dimensions = {
            "pool": "chromium.tests.ioswpt",
        },
    ),
)

targets.mixin(
    name = "isolate_profile_data",
    isolate_profile_data = True,
)

targets.mixin(
    name = "junit-swarming-emulator",
    swarming = targets.swarming(
        dimensions = {
            "cores": "8",
            "pool": "chromium.tests",
        },
    ),
)

targets.mixin(
    name = "limited_capacity_bot",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    # Some FYI bot configurations have a limited number of bots in the swarming
    # pool. Increase the default expiration_sec time from 1 hour to 6 hours to
    # prevent shards from timing out.
    swarming = targets.swarming(
        expiration_sec = 21600,
    ),
)

targets.mixin(
    name = "linux-focal",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        dimensions = {
            "os": "Ubuntu-20.04",
        },
    ),
)

targets.mixin(
    name = "linux-jammy",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "os": "Ubuntu-22.04",
        },
    ),
)

targets.mixin(
    name = "linux-jammy-or-focal",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        dimensions = {
            "os": "Ubuntu-22.04|Ubuntu-20.04",
        },
    ),
)

# TODO(crbug.com/40201775): Remove the xenial mixin once the MSAN bots have
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
    name = "linux-noble",
    swarming = targets.swarming(
        dimensions = {
            "os": "Ubuntu-24.04",
        },
    ),
)

targets.mixin(
    name = "linux_amd_rx_5500_xt",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "gpu": "1002:7340-23.2.1",
            "os": "Ubuntu-22.04",
            "display_attached": "1",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "linux_intel_uhd_630_experimental",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "gpu": "8086:9bc5-23.2.1",
            "os": "Ubuntu-22.04.5",
            "display_attached": "1",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "linux_intel_uhd_630_stable",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "gpu": "8086:9bc5-20.0.8|8086:9bc5-23.2.1",
            "os": "Ubuntu-18.04.6|Ubuntu-22.04",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "linux_intel_uhd_770_stable",
    swarming = targets.swarming(
        dimensions = {
            "gpu": "8086:4680-23.2.1",
            "os": "Ubuntu-22.04.4",
            "display_attached": "1",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "linux_nvidia_gtx_1660_experimental",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "gpu": "10de:2184-535.183.01",
            "os": "Ubuntu-22.04",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "linux_nvidia_gtx_1660_stable",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    # TODO(crbug.com/40888390): The swarming dimensions for
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
    name = "linux_vulkan",
    linux_args = [
        "--extra-browser-args=--enable-features=Vulkan",
    ],
)

targets.mixin(
    name = "lollipop-x86-emulator",
    generate_pyl_entry = False,
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
    name = "mac_14_arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "os": "Mac-14",
        },
    ),
)

targets.mixin(
    name = "mac_14_x64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-14",
        },
    ),
)

targets.mixin(
    name = "mac_14_beta_arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "os": "Mac-14.5",
        },
    ),
)

targets.mixin(
    name = "mac_15_arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "os": "Mac-15",
        },
    ),
)

targets.mixin(
    name = "mac_15_x64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-15",
        },
    ),
)

targets.mixin(
    name = "mac_arm64_apple_m1_gpu_experimental",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "gpu": "apple:m1",
            "mac_model": "Macmini9,1",
            "os": "Mac-14.5",
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
            "gpu": "apple:m1",
            "mac_model": "Macmini9,1",
            "os": "Mac-14.5",
            "pool": "chromium.tests",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_arm64_apple_m2_retina_gpu_experimental",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "gpu": "apple:m2",
            "mac_model": "Mac14,7",
            "os": "Mac-14.4.1",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
            "hidpi": "1",
        },
    ),
)

targets.mixin(
    name = "mac_arm64_apple_m2_retina_gpu_stable",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "gpu": "apple:m2",
            "mac_model": "Mac14,7",
            "os": "Mac-14.4.1",
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
            "os": "Mac-15",
        },
    ),
)

targets.mixin(
    name = "mac_beta_x64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-15",
        },
    ),
)

targets.mixin(
    name = "mac_default_arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "os": "Mac-14",
        },
    ),
)

# mac_default_x64 is used as a prefered OS dimension for mac platform
# instead of any mac OS version. It selects the most representative
# dimension on Swarming.
targets.mixin(
    name = "mac_default_x64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "os": "Mac-14",
        },
    ),
)

targets.mixin(
    name = "mac_mini_intel_gpu_experimental",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "8086:3e9b",
            "os": "Mac-15.0",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_mini_intel_gpu_stable",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    # TODO(crbug.com/40888390): The swarming dimensions for
    # webgpu_blink_web_tests and webgpu_cts_tests on mac-code-coverage
    # must be kept manually in sync with the appropriate mixin; currently,
    # this one, which is used by Dawn Mac x64 Release (Intel).
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "8086:3e9b",
            "os": "Mac-14.5",
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
            "os": "Mac-12.7",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_retina_amd_gpu_experimental",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "1002:67ef",
            "hidpi": "1",
            "os": "Mac-14.4.1",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_retina_amd_gpu_stable",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "1002:7340",
            "hidpi": "1",
            "os": "Mac-14.4.1",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_retina_nvidia_gpu_experimental",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    # Currently the same as the stable version.
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "10de:0fe9",
            "hidpi": "1",
            "os": "Mac-11.7.9",
            "pool": "chromium.tests.gpu",
            "display_attached": "1",
        },
    ),
)

targets.mixin(
    name = "mac_retina_nvidia_gpu_stable",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "gpu": "10de:0fe9",
            "hidpi": "1",
            "os": "Mac-11.7.9",
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
                revision = "git_revision:a18b7d95d26f3c6bf9591978b19cf0ca8268ac7d",
            ),
        ],
    ),
)

targets.mixin(
    name = "marshmallow",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        dimensions = {
            "device_os": "MMB29Q",
        },
    ),
)

targets.mixin(
    name = "marshmallow-x86-emulator",
    generate_pyl_entry = False,
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

targets.mixin(
    name = "motorola_moto_g_power_5g",
    swarming = targets.swarming(
        dimensions = {
            "device_type": "devonn",
            "device_os": "T",
            "device_os_flavor": "motorola",
            "device_os_type": "user",
            "os": "Android",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "no_gpu",
    generate_pyl_entry = targets.IGNORE_UNUSED,
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
    name = "nvidia_geforce_gtx_1660",
    swarming = targets.swarming(
        dimensions = {
            "gpu": "10de:2184",
        },
    ),
)

targets.mixin(
    name = "nougat-x86-emulator",
    generate_pyl_entry = False,
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
        "--avd-config=../../tools/android/avd/proto/generic_android26.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "generic_android26",
            },
        },
        named_caches = [
            swarming.cache(
                name = "generic_android26",
                path = ".android_emulator/generic_android26",
            ),
        ],
    ),
)

targets.mixin(
    name = "oreo_mr1_fleet",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        dimensions = {
            "device_os": "OPM4.171019.021.P2",
            "device_os_flavor": "google",
        },
    ),
)

# Pixel 8
targets.mixin(
    name = "shiba",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        dimensions = {
            "device_type": "shiba",
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

# Pixel 7 on Android 14
targets.mixin(
    name = "panther_on_14",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        dimensions = {
            "device_type": "panther",
            "device_os": "AP2A.240705.004",  # Android 14
            "os": "Android",
        },
    ),
)

targets.mixin(
    name = "pie-x86-emulator",
    generate_pyl_entry = False,
    args = [
        "--avd-config=../../tools/android/avd/proto/android_28_google_apis_x86.textpb",
    ],
    swarming = targets.swarming(
        # soft affinity so that bots with caches will be picked first
        optional_dimensions = {
            60: {
                "caches": "android_28_google_apis_x86",
            },
        },
        named_caches = [
            swarming.cache(
                name = "android_28_google_apis_x86",
                path = ".android_emulator/android_28_google_apis_x86",
            ),
        ],
    ),
)

targets.mixin(
    name = "puppet_production",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        dimensions = {
            "puppet_env": "production",
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
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        containment_type = "AUTO",
    ),
)

targets.mixin(
    name = "skylab-cft",
    skylab = targets.skylab(
        run_cft = True,
    ),
)

targets.mixin(
    name = "cros-cbx-dut",
    skylab = targets.skylab(
        cros_cbx = True,
    ),
)

# Pixel Tablet
targets.mixin(
    name = "tangorpro",
    generate_pyl_entry = False,
    swarming = targets.swarming(
        dimensions = {
            "device_type": "tangorpro",
            "device_os": "AP1A.240505.005",  # Android 14
            "os": "Android",
        },
    ),
)

targets.mixin(
    name = "timeout_15m",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        hard_timeout_sec = 900,
        io_timeout_sec = 900,
    ),
)

targets.mixin(
    name = "timeout_30m",
    swarming = targets.swarming(
        hard_timeout_sec = 1800,
        io_timeout_sec = 1800,
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
    name = "vaapi_unittest_args",
    args = [
        "--stop-ui",
        "--gtest_filter=\"VaapiTest.*\"",
    ],
)

targets.mixin(
    name = "vaapi_unittest_libfake_args",
    args = [
        # Tell libva to do dummy encoding/decoding. For more info, see:
        # https://github.com/intel/libva/blob/v2.14-branch/va/va_fool.c#L52
        "--env-var",
        "LIBVA_DRIVERS_PATH",
        "./",
        "--env-var",
        "LIBVA_DRIVER_NAME",
        "libfake",
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
    name = "web-test-leak",
    args = [
        "--additional-expectations",
        "../../third_party/blink/web_tests/LeakExpectations",
        "--enable-leak-detection",
    ],
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
    ],
)

targets.mixin(
    name = "webview_cts_archive",
    swarming = targets.swarming(
        cipd_packages = [
            targets.cipd_package(
                package = "chromium/android_webview/tools/cts_archive",
                location = "android_webview/tools/cts_archive/cipd",
                revision = "UYQZhJpB3MWpJIAcesI-M1bqRoTghiKCYr_SD9tPDewC",
            ),
        ],
    ),
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
    name = "win10_amd_rx_5500_xt_stable",
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            "gpu": "1002:7340-31.0.24002.92",
            "os": "Windows-10-19045.3930",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "win10_gce_gpu_pool",
    generate_pyl_entry = targets.IGNORE_UNUSED,
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
            "gpu": "8086:9bc5-31.0.101.2127",
            "os": "Windows-10",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "win10_intel_uhd_630_stable",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            "gpu": "8086:9bc5-31.0.101.2127",
            "os": "Windows-10",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "win10_intel_uhd_770_stable",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            "gpu": "8086:4680-31.0.101.5333",
            "os": "Windows-10-19045.3930",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "win10_nvidia_gtx_1660_experimental",
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            "gpu": "10de:2184-31.0.15.4601",
            "os": "Windows-10-19045",
            "pool": "chromium.tests.gpu",
        },
    ),
)

targets.mixin(
    name = "win10_nvidia_gtx_1660_stable",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    # TODO(crbug.com/40888390): The swarming dimensions for
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

targets.mixin(
    name = "win10_nvidia_rtx_4070_super_stable",
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            "gpu": "10de:2783",
            "os": "Windows-10",
            "pool": "chromium.tests.gpu.experimental",
        },
    ),
)

targets.mixin(
    name = "win11_qualcomm_adreno_690_stable",
    swarming = targets.swarming(
        dimensions = {
            "display_attached": "1",
            # Screen scaling is mostly to ensure that pixel test output is
            # consistent.
            "screen_scaling_percent": "100",
            "cpu": "arm64",
            "gpu": "qcom:043a-27.20.1870.0",
            "os": "Windows-11-22631",
            "pool": "chromium.tests",
        },
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
    name = "win11-23h2",
    swarming = targets.swarming(
        dimensions = {
            "os": "Windows-11-22631",
        },
        expiration_sec = 36000,
    ),
)

targets.mixin(
    name = "win11-any",
    swarming = targets.swarming(
        dimensions = {
            "os": "Windows-11",
        },
    ),
)

targets.mixin(
    name = "win-arm64",
    swarming = targets.swarming(
        dimensions = {
            # Certain tests require 100 percent screen scaling, and all devices
            # should be configured for this.
            "screen_scaling_percent": "100",
            "cpu": "arm64",
            "os": "Windows-11",
        },
    ),
)

targets.mixin(
    name = "x86-64",
    generate_pyl_entry = targets.IGNORE_UNUSED,
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
        },
    ),
)

targets.mixin(
    name = "xcode_15_beta",
    args = [
        "--xcode-build-version",
        "15f31d",
    ],
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "xcode_ios_15f31d",
                path = "Xcode.app",
            ),
        ],
    ),
)

targets.mixin(
    name = "xcode_16_main",
    args = [
        "--xcode-build-version",
        "16a242d",
    ],
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "xcode_ios_16a242d",
                path = "Xcode.app",
            ),
        ],
    ),
)

targets.mixin(
    name = "xcode_16_beta",
    args = [
        "--xcode-build-version",
        "16a242d",
    ],
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "xcode_ios_16a242d",
                path = "Xcode.app",
            ),
        ],
    ),
)

targets.mixin(
    name = "xcode_16_1_beta",
    args = [
        "--xcode-build-version",
        "16b5029d",
    ],
    swarming = targets.swarming(
        named_caches = [
            swarming.cache(
                name = "xcode_ios_16b5029d",
                path = "Xcode.app",
            ),
        ],
    ),
)

targets.mixin(
    name = "xcodebuild_sim_runner",
    args = [
        "--xcodebuild-sim-runner",
    ],
)

targets.mixin(
    name = "xctest",
    args = [
        "--xctest",
    ],
)
