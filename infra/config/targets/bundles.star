# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains bundle definitions, which are groupings of targets that can
# be referenced by other bundles or by builders. Bundles cannot be used in
# //testing/buildbot

load("//lib/targets.star", "targets")

targets.bundle(
    name = "android_10_rel_gtests",
    targets = [
        "android_ar_gtests",
        "android_trichrome_smoke_tests",
        "vr_android_specific_chromium_tests",
    ],
)

targets.bundle(
    name = "android_11_emulator_gtests",
    targets = [
        "android_emulator_specific_chrome_public_tests",
        "android_trichrome_smoke_tests",
        "android_smoke_tests",
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",  # Not an experimental test
        targets.bundle(
            targets = "webview_trichrome_cts_tests_suite",
            variants = [
                "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
                "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
            ],
        ),
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_12_dbg_emulator_gtests",
    targets = [
        "android_trichrome_smoke_tests",
    ],
)

targets.bundle(
    name = "android_12_emulator_gtests",
    targets = [
        "android_ci_only_fieldtrial_webview_tests",
        "android_emulator_specific_chrome_public_tests",
        "android_trichrome_smoke_tests",
        "android_smoke_tests",
        # Already includes gl_gtests.
        "android_specific_chromium_gtests",
        "chrome_profile_generator_tests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "fieldtrial_android_tests",
        "jni_zero_sample_apk_test",
        "linux_flavor_specific_chromium_gtests",
        "minidump_uploader_test",
        "system_webview_shell_instrumentation_tests",  # Not an experimental test
        targets.bundle(
            targets = "webview_trichrome_64_cts_tests_suite",
            variants = [
                "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
                "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
            ],
        ),
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_12l_emulator_gtests",
    targets = [
        "android_emulator_specific_chrome_public_tests",
        "android_trichrome_smoke_tests",
        "android_smoke_tests",
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",  # Not an experimental test
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_12l_landscape_emulator_gtests",
    targets = [
        "android_emulator_specific_chrome_public_tests",
    ],
)

targets.bundle(
    name = "android_12l_rel_cq_gtests",
    targets = [
        "tablet_sensitive_chrome_public_test_apk",
    ],
    per_test_modifications = {
        "tablet_sensitive_chrome_public_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "android_13_emulator_gtests",
    targets = [
        "android_ci_only_fieldtrial_webview_tests",
        "android_emulator_specific_chrome_public_tests",
        "android_trichrome_smoke_tests",
        "android_smoke_tests",
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chrome_profile_generator_tests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "fieldtrial_android_tests",
        "jni_zero_sample_apk_test",
        "linux_flavor_specific_chromium_gtests",
        "minidump_uploader_test",
        "system_webview_shell_instrumentation_tests",  # Not an experimental test
        targets.bundle(
            targets = "webview_trichrome_64_cts_tests_suite",
            variants = [
                "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
                "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
            ],
        ),
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_14_device_fyi_gtests",
    targets = [
        "system_webview_shell_instrumentation_tests",
        targets.bundle(
            targets = "webview_trichrome_64_cts_tests_suite",
            variants = [
                "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
                "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
            ],
        ),
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_14_device_gtests",
    targets = [
        "android_hardware_specific_gtests",
        "android_limited_capacity_gtests",
        "android_trichrome_smoke_tests",
        "android_smoke_tests",
        "chrome_public_tests",
    ],
)

targets.bundle(
    name = "android_14_emulator_gtests",
    targets = [
        "android_emulator_specific_chrome_public_tests",
        "android_trichrome_smoke_tests",
        "android_smoke_tests",
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",  # Not an experimental test
        targets.bundle(
            targets = "webview_trichrome_64_cts_tests_suite",
            variants = [
                "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
                "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
            ],
        ),
        "webview_trichrome_64_cts_tests_no_field_trial_suite",
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_14_tablet_gtests",
    targets = [
        "android_trichrome_smoke_tests",
        "android_smoke_tests",
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chrome_public_tests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",  # Not an experimental test
        targets.bundle(
            targets = "webview_trichrome_64_cts_tests_suite",
            variants = [
                "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
                "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
            ],
        ),
        "webview_trichrome_64_cts_tests_no_field_trial_suite",
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_15_emulator_fyi_gtests",
    targets = [
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chromium_gtests",
        "android_emulator_specific_chrome_public_tests",
        "android_trichrome_smoke_tests",
        "android_smoke_tests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",  # Not an experimental test
        "webview_ui_instrumentation_tests",
        targets.bundle(
            targets = "webview_trichrome_64_cts_tests_suite",
            variants = [
                "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
                "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
            ],
        ),
        "webview_trichrome_64_cts_tests_no_field_trial_suite",
    ],
)

targets.bundle(
    name = "android_15_emulator_gtests",
    targets = [
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chromium_gtests",
        "android_emulator_specific_chrome_public_tests",
        "android_trichrome_smoke_tests",
        "android_smoke_tests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",  # Not an experimental test
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_ar_gtests",
    targets = [
        "monochrome_public_test_ar_apk",
        # Name is vr_*, but actually has AR tests.
        "vr_android_unittests",
    ],
)

targets.bundle(
    name = "android_browsertests_fyi",
    targets = [
        "android_browsertests",
    ],
    per_test_modifications = {
        "android_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 24,
            ),
        ),
    },
)

targets.bundle(
    name = "android_ci_only_fieldtrial_webview_tests",
    targets = [
        "webview_trichrome_64_cts_tests_no_field_trial",
        "webview_ui_test_app_test_apk_no_field_trial",
    ],
    mixins = [
        "ci_only",
    ],
    per_test_modifications = {
        "webview_trichrome_64_cts_tests_no_field_trial": targets.mixin(
            args = [
                "--store-tombstones",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "android_content_browsertests_fyi",
    targets = [
        "content_browsertests",
    ],
    per_test_modifications = {
        "content_browsertests": targets.mixin(
            android_swarming = targets.swarming(
                shards = 15,
            ),
        ),
    },
)

targets.bundle(
    name = "android_cronet_clang_coverage_gtests",
    targets = [
        "cronet_clang_coverage_additional_gtests",
        "cronet_gtests",
    ],
)

# Android desktop tests that run on a Linux host.
targets.bundle(
    name = "android_desktop_junit_tests",
    targets = [
        "chrome_junit_tests",
    ],
    mixins = [
        "has_native_resultdb_integration",
        "junit-swarming-emulator",
        "linux-jammy",
        "x86-64",
    ],
)

# Android desktop tests that run on AVDs or devices. Specific emulator or
# device mixins should be added where this is used.
targets.bundle(
    name = "android_desktop_tests",
    targets = [
        "android_browsertests",
        "chrome_public_test_apk",
        "chrome_public_unit_test_apk",
        "extensions_unittests",
        "unit_tests",
    ],
    mixins = [
        "has_native_resultdb_integration",
        "linux-jammy",
        "x86-64",
    ],
    per_test_modifications = {
        "chrome_public_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 15,
            ),
            experiment_percentage = 100,
        ),
        "chrome_public_unit_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
            experiment_percentage = 100,
        ),
        "android_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 5,
            ),
            experiment_percentage = 100,
        ),
        "unit_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
            experiment_percentage = 100,
        ),
    },
)

targets.bundle(
    name = "android_emulator_specific_chrome_public_tests",
    targets = [
        "chrome_public_test_apk",
        "chrome_public_unit_test_apk",
    ],
    per_test_modifications = {
        "chrome_public_test_apk": [
            targets.mixin(
                swarming = targets.swarming(
                    shards = 20,
                ),
            ),
            "emulator-8-cores",
        ],
        "chrome_public_unit_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
    },
)

targets.bundle(
    name = "android_emulator_specific_network_enabled_content_browsertests",
    targets = [
        "content_browsertests_with_emulator_network",
    ],
)

targets.bundle(
    name = "android_fieldtrial_rel_webview_tests",
    targets = [
        "fieldtrial_android_tests",
        targets.bundle(
            targets = "system_webview_shell_instrumentation_tests",
            variants = [
                "DISABLE_FIELD_TRIAL_CONFIG_WEBVIEW_COMMANDLINE",
                "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR_WEBVIEW_COMMANDLINE",
                "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR_WEBVIEW_COMMANDLINE",
            ],
        ),
        targets.bundle(
            targets = "webview_bot_instrumentation_test_apk_gtest",
            variants = [
                "DISABLE_FIELD_TRIAL_CONFIG",
                "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR",
                "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR",
            ],
        ),
        targets.bundle(
            targets = "webview_trichrome_64_cts_field_trial_tests",
            variants = [
                "DISABLE_FIELD_TRIAL_CONFIG",
                "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR",
                "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR",
            ],
        ),
        targets.bundle(
            targets = "webview_ui_instrumentation_tests",
            variants = [
                "DISABLE_FIELD_TRIAL_CONFIG",
                "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR",
                "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR",
            ],
        ),
    ],
)

# Test suites that need to run on hardware that is close to real Android device.
# See https://crbug.com/40204012#comment5 for details.
targets.bundle(
    name = "android_hardware_specific_gtests",
    targets = [
        "cc_unittests",
        "viz_unittests",
    ],
)

targets.bundle(
    name = "android_isolated_scripts",
    targets = [
        "content_shell_crash_test",
    ],
    per_test_modifications = {
        "content_shell_crash_test": targets.mixin(
            args = [
                "--platform=android",
            ],
        ),
    },
)

# Used when the device capacity is limited, e.g. for CQ.
# TODO(crbug.com/352811552): Revisit after Android 14 on device promoted to CQ.
targets.bundle(
    name = "android_limited_capacity_gtests",
    targets = [
        "android_browsertests",
        "blink_platform_unittests",
        "content_browsertests",
        "webview_instrumentation_test_apk_multiple_process_mode",
    ],
    per_test_modifications = {
        "content_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
            swarming = targets.swarming(
                shards = 6,
            ),
        ),
    },
)

targets.bundle(
    name = "android_marshmallow_gtests",
    targets = [
        "android_smoke_tests",
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chrome_public_tests",
        "linux_flavor_specific_chromium_gtests",
        "vr_android_specific_chromium_tests",
        "vr_platform_specific_chromium_gtests",
        "webview_instrumentation_test_apk_single_process_mode_gtests",
    ],
)

targets.bundle(
    name = "android_monochrome_smoke_tests",
    targets = [
        "monochrome_public_bundle_smoke_test",
        "monochrome_public_smoke_test",
    ],
)

targets.bundle(
    name = "android_oreo_emulator_gtests",
    targets = [
        "android_emulator_specific_chrome_public_tests",
        "android_emulator_specific_network_enabled_content_browsertests",
        "android_monochrome_smoke_tests",
        "android_smoke_tests",
        "android_specific_chromium_gtests",
        "android_wpr_record_replay_tests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",
        "webview_cts_tests_gtest",
        "webview_instrumentation_test_apk_single_process_mode_gtests",
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_oreo_gtests",
    targets = [
        "android_ar_gtests",
        "vr_android_specific_chromium_tests",
        "android_monochrome_smoke_tests",
        "android_oreo_standard_gtests",
        "android_smoke_tests",
    ],
)

targets.bundle(
    name = "android_oreo_standard_gtests",
    targets = [
        "chrome_public_test_apk",
        "chrome_public_unit_test_apk",
        "webview_instrumentation_test_apk",
    ],
    per_test_modifications = {
        "chrome_public_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
        "webview_instrumentation_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 5,
                expiration_sec = 10800,
            ),
        ),
    },
)

targets.bundle(
    name = "android_pie_coverage_instrumentation_tests",
    targets = [
        "android_smoke_tests",
        "android_specific_coverage_java_tests",
        "chrome_public_tests",
        "vr_android_specific_chromium_tests",
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_pie_gtests",
    targets = [
        "android_ar_gtests",
        "vr_android_specific_chromium_tests",
        "android_monochrome_smoke_tests",
        "android_smoke_tests",
        "chromium_tracing_gtests",
        "android_pie_standard_gtests",
        # No standard tests due to capacity, no Vega tests since it's currently
        # O only.
    ],
)

targets.bundle(
    name = "android_pie_standard_gtests",
    targets = [
        "chrome_public_test_apk",
        "chrome_public_unit_test_apk",
        "webview_instrumentation_test_apk",
    ],
    per_test_modifications = {
        "chrome_public_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "webview_instrumentation_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 6,
            ),
        ),
    },
)

# Keep in sync with android_pie_rel_gtests, except for
# vr_{android,platform}_specific_chromium_gtests which are not applicable
# to android emulators on x86 & x64.
targets.bundle(
    name = "android_pie_rel_emulator_gtests",
    targets = [
        "android_emulator_specific_chrome_public_tests",
        "android_monochrome_smoke_tests",
        "android_smoke_tests",
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",  # Not an experimental test
        "webview_cts_tests_gtest",
        "webview_ui_instrumentation_tests",
        "webview_instrumentation_test_apk_single_process_mode_gtests",
    ],
)

targets.bundle(
    name = "android_pie_emulator_gtests",
    targets = [
        "android_emulator_specific_chrome_public_tests",
        "android_emulator_specific_network_enabled_content_browsertests",
        "android_monochrome_smoke_tests",
        "android_smoke_tests",
        "android_specific_chromium_gtests",
        "android_wpr_record_replay_tests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",
        "webview_cts_tests_gtest",
        "webview_instrumentation_test_apk_single_process_mode_gtests",
        "webview_ui_instrumentation_tests",
    ],
)

targets.bundle(
    name = "android_pie_rel_gtests",
    targets = [
        # TODO(crbug.com/40142574): Deprecate this when all the test suites below
        # it are re-enabled.
        "android_pie_rel_reduced_capacity_gtests",
        "android_monochrome_smoke_tests",
        "android_smoke_tests",
        # "android_specific_chromium_gtests",  # Already includes gl_gtests.
        # "chromium_gtests",
        # "chromium_gtests_for_devices_with_graphical_output",
        "chrome_public_tests",
        # "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",
        # "vr_android_specific_chromium_tests",
        # "vr_platform_specific_chromium_gtests",
        "webview_64_cts_tests_suite",
        "webview_instrumentation_test_apk_single_process_mode_gtests",
        "webview_ui_instrumentation_tests",
    ],
)

# TODO(crbug.com/40142574): Deprecate this group in favor of
# android_pie_rel_gtests if/when android Pie capacity is fully restored.
targets.bundle(
    name = "android_pie_rel_reduced_capacity_gtests",
    targets = [
        "android_browsertests",
        "blink_platform_unittests",
        "cc_unittests",
        "content_browsertests",
        "viz_unittests",
        "webview_instrumentation_test_apk_multiple_process_mode",
    ],
    per_test_modifications = {
        "content_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
    },
)

targets.bundle(
    name = "android_rel_isolated_scripts",
    targets = [
        "private_code_failure_test",
        "android_blink_wpt_tests",
        "webview_blink_wpt_tests",
    ],
    per_test_modifications = {
        "android_blink_wpt_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
        "webview_blink_wpt_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
    },
)

targets.bundle(
    name = "android_smoke_tests",
    targets = [
        "chrome_public_smoke_test",
    ],
)

targets.bundle(
    name = "android_specific_chromium_gtests",
    targets = [
        "android_browsertests",
        "android_sync_integration_tests",
        "android_webview_unittests",
        "content_shell_test_apk",
        "mojo_test_apk",
        "ui_android_unittests",
        "webview_instrumentation_test_apk_multiple_process_mode",
        # TODO(kbr): these are actually run on many of the GPU bots, which have
        # physical hardware for several of the desktop OSs. Once the GPU JSON
        # generation script is merged with this one, this should be promoted from
        # the Android-specific section.
        "gl_tests_validating",
        "gl_unittests",
    ],
    per_test_modifications = {
        "android_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
        "android_sync_integration_tests": targets.mixin(
            args = [
                "--test-launcher-batch-limit=1",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
        "content_shell_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
    },
)

targets.bundle(
    name = "android_specific_coverage_java_tests",
    targets = [
        "content_shell_test_apk",
        "mojo_test_apk",
        "webview_instrumentation_test_apk_multiple_process_mode",
        "webview_instrumentation_test_apk_single_process_mode",
    ],
    per_test_modifications = {
        "content_shell_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "webview_instrumentation_test_apk_multiple_process_mode": targets.mixin(
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
        "webview_instrumentation_test_apk_single_process_mode": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
    },
)

targets.bundle(
    name = "android_trichrome_smoke_tests",
    targets = [
        "trichrome_chrome_bundle_smoke_test",
    ],
)

targets.bundle(
    name = "android_webview_gpu_telemetry_tests",
    targets = [
        "android_webview_pixel_skia_gold_test",
    ],
    per_test_modifications = {
        "android_webview_pixel_skia_gold_test": [
            targets.mixin(
                args = [
                    "--dont-restore-color-profile-after-test",
                    "--test-machine-name",
                    "${buildername}",
                ],
            ),
            "gpu_integration_test_common_args",
        ],
    },
)

targets.bundle(
    name = "android_wpr_record_replay_tests",
    targets = [
        "chrome_java_test_wpr_tests",
    ],
)

targets.bundle(
    name = "ash_pixel_gtests",
    targets = [
        "ash_pixeltests",
    ],
)

targets.bundle(
    name = "bfcache_android_gtests",
    targets = [
        "bf_cache_android_browsertests",
        "bfcache_generic_gtests",
        "webview_instrumentation_test_apk_bfcache_mutations",
        "webview_cts_tests_bfcache_mutations",
    ],
    per_test_modifications = {
        "bf_cache_android_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "webview_cts_tests_bfcache_mutations": targets.mixin(
            args = [
                "--store-tombstones",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "webview_instrumentation_test_apk_bfcache_mutations": targets.mixin(
            swarming = targets.swarming(
                shards = 12,
            ),
        ),
    },
)

# Run content_browser_tests with BackForwardCache disabled
targets.bundle(
    name = "bfcache_generic_gtests",
    targets = [
        "bf_cache_content_browsertests",
    ],
    per_test_modifications = {
        "bf_cache_content_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
    },
)

# Run browser_tests with BackForwardCache disabled
targets.bundle(
    name = "bfcache_linux_gtests",
    targets = [
        "bfcache_generic_gtests",
        "bfcache_linux_specific_gtests",
    ],
)

targets.bundle(
    name = "bfcache_linux_specific_gtests",
    targets = [
        "bf_cache_browser_tests",
    ],
    per_test_modifications = {
        "bf_cache_browser_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
    },
)

targets.bundle(
    name = "cast_junit_tests",
    targets = [
        "cast_base_junit_tests",
        "cast_shell_junit_tests",
    ],
    mixins = [
        "x86-64",
        "linux-jammy",
        "junit-swarming-emulator",
    ],
)

targets.bundle(
    name = "cast_receiver_gtests",
    additional_compile_targets = [
        "cast_test_lists",
    ],
    targets = [
        "cast_audio_backend_unittests",
        "cast_base_unittests",
        "cast_cast_core_unittests",
        "cast_media_unittests",
        "cast_unittests",
    ],
    mixins = [
        "linux-jammy",
    ],
)

targets.bundle(
    name = "cast_receiver_junit_tests",
    additional_compile_targets = [
        "cast_junit_test_lists",
    ],
    targets = [
        "base_junit_tests",
        "cast_base_junit_tests",
        "cast_shell_junit_tests",
        "content_junit_tests",
        "net_junit_tests",
    ],
    mixins = [
        "x86-64",
        "linux-jammy",
        "junit-swarming-emulator",
    ],
)

targets.bundle(
    name = "chrome_profile_generator_tests",
    targets = [
        "chrome_public_apk_profile_tests",
    ],
    per_test_modifications = {
        "chrome_public_apk_profile_tests": targets.mixin(
            ci_only = True,
            experiment_percentage = 100,
        ),
    },
)

targets.bundle(
    name = "chrome_public_tests",
    targets = [
        "chrome_public_test_apk",
        "chrome_public_unit_test_apk",
    ],
    per_test_modifications = {
        "chrome_public_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 19,
            ),
        ),
        "chrome_public_unit_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "chrome_public_wpt_suite",
    targets = "chrome_public_wpt",
    per_test_modifications = {
        "chrome_public_wpt": targets.mixin(
            args = [
                "--no-wpt-internal",
            ],
            swarming = targets.swarming(
                shards = 36,
                expiration_sec = 18000,
                hard_timeout_sec = 14400,
            ),
        ),
    },
)

targets.bundle(
    name = "chrome_sizes_android",
    targets = [
        "chrome_sizes",
    ],
    per_test_modifications = {
        "chrome_sizes": targets.per_test_modification(
            mixins = targets.mixin(
                args = [
                    "--platform=android",
                ],
                swarming = targets.swarming(
                    dimensions = {
                        "cpu": "x86-64",
                        "os": "Ubuntu-22.04",
                    },
                ),
            ),
            remove_mixins = [
                "chromium_nexus_5x_oreo",
                "chromium_pixel_2_pie",
                "marshmallow",
                "oreo_mr1_fleet",
            ],
        ),
    },
)

targets.bundle(
    name = "chromeos_annotation_scripts",
    targets = [
        "check_network_annotations",
    ],
)

targets.bundle(
    name = "chromeos_arm_gtests",
    targets = [
        "video_decode_accelerator_tests_v4l2_vp8",
        "video_decode_accelerator_tests_v4l2_vp9",
    ],
    per_test_modifications = {
        "video_decode_accelerator_tests_v4l2_vp8": targets.mixin(
            ci_only = True,
            # TODO(crbug.com/303119905): Remove experimental status first.
            # Then promote out of ci-only optionally.
            experiment_percentage = 100,
        ),
        "video_decode_accelerator_tests_v4l2_vp9": targets.mixin(
            ci_only = True,
            # TODO(crbug.com/303119905): Remove experimental status first.
            # Then promote out of ci-only optionally.
            experiment_percentage = 100,
        ),
    },
)

targets.bundle(
    name = "chromeos_browser_all_tast_tests",
    targets = [
        "chrome_all_tast_tests",
    ],
    per_test_modifications = {
        "chrome_all_tast_tests": [
            targets.mixin(
                args = [
                    "--tast-retries=1",
                ],
                swarming = targets.swarming(
                    shards = 10,
                    # Tast test doesn't always output. See crbug.com/1306300
                    io_timeout_sec = 3600,
                    # https://crbug.com/923426#c27
                    idempotent = False,
                ),
            ),
            "has_native_resultdb_integration",
        ],
    },
)

# Test suite for running criticalstaging Tast tests.
targets.bundle(
    name = "chromeos_browser_criticalstaging_tast_tests",
    targets = [
        "chrome_criticalstaging_tast_tests",
    ],
    per_test_modifications = {
        "chrome_criticalstaging_tast_tests": [
            targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 2,
                    # Tast test doesn't always output. See crbug.com/1306300
                    io_timeout_sec = 3600,
                    # https://crbug.com/923426#c27
                    idempotent = False,
                ),
                experiment_percentage = 100,
            ),
            "has_native_resultdb_integration",
        ],
    },
)

# Test suite for running disabled Tast tests to collect data to re-enable
# them. The test suite should not be critical to builders.
targets.bundle(
    name = "chromeos_browser_disabled_tast_tests",
    targets = [
        "chrome_disabled_tast_tests",
    ],
    per_test_modifications = {
        "chrome_disabled_tast_tests": [
            targets.mixin(
                ci_only = True,
                swarming = targets.swarming(
                    shards = 2,
                    # Tast test doesn't always output. See crbug.com/1306300
                    io_timeout_sec = 3600,
                    # https://crbug.com/923426#c27
                    idempotent = False,
                ),
                experiment_percentage = 100,
            ),
            "has_native_resultdb_integration",
        ],
    },
)

targets.bundle(
    name = "chromeos_browser_integration_tests",
    targets = [
        "disk_usage_tast_test",
    ],
    per_test_modifications = {
        "disk_usage_tast_test": [
            targets.mixin(
                args = [
                    # Stripping gives more accurate disk usage data.
                    "--strip-chrome",
                ],
                swarming = targets.swarming(
                    # https://crbug.com/923426#c27
                    idempotent = False,
                ),
            ),
            "has_native_resultdb_integration",
        ],
    },
)

targets.bundle(
    name = "chromeos_isolated_scripts",
    targets = [
        "telemetry_perf_unittests",
        "telemetry_unittests",
    ],
    per_test_modifications = {
        "telemetry_perf_unittests": [
            targets.mixin(
                args = [
                    "--browser=cros-chrome",
                    targets.magic_args.CROS_TELEMETRY_REMOTE,
                    "--xvfb",
                    # 3 is arbitrary, but if we're having more than 3 of these tests
                    # fail in a single shard, then something is probably wrong, so fail
                    # fast.
                    "--typ-max-failures=3",
                ],
                swarming = targets.swarming(
                    shards = 12,
                    # https://crbug.com/549140
                    idempotent = False,
                ),
            ),
            "has_native_resultdb_integration",
        ],
        "telemetry_unittests": [
            targets.mixin(
                args = [
                    "--jobs=1",
                    "--browser=cros-chrome",
                    targets.magic_args.CROS_TELEMETRY_REMOTE,
                    # 3 is arbitrary, but if we're having more than 3 of these tests
                    # fail in a single shard, then something is probably wrong, so fail
                    # fast.
                    "--typ-max-failures=3",
                ],
                swarming = targets.swarming(
                    shards = 24,
                    # https://crbug.com/549140
                    idempotent = False,
                ),
            ),
            "has_native_resultdb_integration",
        ],
    },
)

targets.bundle(
    name = "chromeos_js_code_coverage_browser_tests_suite",
    targets = [
        "chromeos_js_code_coverage_browser_tests",
    ],
    per_test_modifications = {
        "chromeos_js_code_coverage_browser_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 32,
            ),
        ),
    },
)

targets.bundle(
    name = "chromeos_vaapi_fakelib_gtests",
    targets = [
        "vaapi_unittest",
    ],
    per_test_modifications = {
        "vaapi_unittest": [
            "vaapi_unittest_args",
            "vaapi_unittest_libfake_args",
        ],
    },
)

targets.bundle(
    name = "chromeos_vm_gtests",
    targets = [
        "chromeos_integration_tests_suite",
        "chromeos_system_friendly_gtests",
        "chromeos_vaapi_fakelib_gtests",
    ],
)

targets.bundle(
    name = "chromeos_vm_tast",
    targets = [
        "chromeos_browser_all_tast_tests",
        "chromeos_browser_criticalstaging_tast_tests",
        "chromeos_browser_disabled_tast_tests",
        "chromeos_browser_integration_tests",
    ],
)

targets.bundle(
    name = "chromium_android_cast_receiver",
    additional_compile_targets = [
        "cast_browser_apk",
    ],
)

targets.bundle(
    name = "chromium_android_cast_receiver_arm64_gtests",
    targets = [
        "cast_android_cma_backend_unittests",
        "cast_receiver_gtests",
    ],
    mixins = [
        "tangorpro",
    ],
)

targets.bundle(
    name = "chromium_android_cast_receiver_arm_gtests",
    targets = [
        "cast_android_cma_backend_unittests",
        "cast_receiver_gtests",
    ],
    mixins = [
        "chromium_pixel_2_pie",
    ],
)

targets.bundle(
    name = "chromium_android_gtests",
    targets = [
        "android_smoke_tests",
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chrome_public_tests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "vr_android_specific_chromium_tests",
        "vr_platform_specific_chromium_gtests",
        "webview_instrumentation_test_apk_single_process_mode_gtests",
    ],
)

targets.bundle(
    name = "chromium_android_scripts",
    targets = [
        "check_network_annotations",
    ],
)

targets.bundle(
    name = "chromium_android_webkit_gtests",
    targets = [
        "blink_heap_unittests",
        "webkit_unit_tests",
    ],
)

targets.bundle(
    name = "chromium_dbg_isolated_scripts",
    targets = [
        "desktop_chromium_isolated_scripts",
        "performance_smoke_test_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

targets.bundle(
    name = "chromium_dev_android_gtests",
    targets = [
        "chrome_public_smoke_test",
    ],
)

targets.bundle(
    name = "chromium_dev_linux_gtests",
    targets = [
        "base_unittests",
        "browser_tests",
        "content_browsertests",
        "content_unittests",
        "interactive_ui_tests",
        "net_unittests",
        "rust_gtest_interop_unittests",
        "unit_tests",
    ],
    per_test_modifications = {
        "base_unittests": targets.mixin(
            swarming = targets.swarming(
                dimensions = {
                    "cores": "8",
                },
            ),
        ),
        "browser_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 8,
                dimensions = {
                    "cores": "8",
                },
            ),
        ),
        "content_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 5,
                dimensions = {
                    "cores": "8",
                },
            ),
        ),
        "content_unittests": targets.mixin(
            swarming = targets.swarming(
                dimensions = {
                    "cores": "2",
                },
            ),
        ),
        "interactive_ui_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
                dimensions = {
                    "cores": "8",
                },
            ),
        ),
        "net_unittests": targets.mixin(
            swarming = targets.swarming(
                dimensions = {
                    "cores": "8",
                },
            ),
        ),
        "unit_tests": targets.mixin(
            swarming = targets.swarming(
                dimensions = {
                    "cores": "2",
                },
            ),
        ),
    },
)

targets.bundle(
    name = "chromium_dev_mac_gtests",
    targets = [
        "base_unittests",
        "content_unittests",
        "net_unittests",
        "rust_gtest_interop_unittests",
        "unit_tests",
    ],
)

targets.bundle(
    name = "chromium_dev_win_gtests",
    targets = [
        "base_unittests",
        "content_browsertests",
        "content_unittests",
        "interactive_ui_tests",
        "net_unittests",
        "rust_gtest_interop_unittests",
        "unit_tests",
    ],
    per_test_modifications = {
        "content_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
        "interactive_ui_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
    },
)

targets.bundle(
    name = "chromium_gtests_for_linux_and_mac_only",
    targets = [
        "openscreen_unittests",
    ],
)

targets.bundle(
    name = "chromium_gtests_for_linux_only",
    targets = [
        "ozone_unittests",
        "ozone_x11_unittests",
    ],
)

targets.bundle(
    name = "chromium_ios_scripts",
    targets = [
        "check_static_initializers",
    ],
)

targets.bundle(
    name = "chromium_junit_tests_scripts",
    targets = [
        "android_webview_junit_tests",
        "base_junit_tests",
        "build_junit_tests",
        "chrome_java_test_pagecontroller_junit_tests",
        "chrome_junit_tests",
        "components_junit_tests",
        "content_junit_tests",
        "device_junit_tests",
        "junit_unit_tests",
        "keyboard_accessory_junit_tests",
        "media_base_junit_tests",
        "module_installer_junit_tests",
        "net_junit_tests",
        "paint_preview_junit_tests",
        "password_check_junit_tests",
        "password_manager_junit_tests",
        "services_junit_tests",
        "touch_to_fill_junit_tests",
        "ui_junit_tests",
        "webapk_client_junit_tests",
        "webapk_shell_apk_h2o_junit_tests",
        "webapk_shell_apk_junit_tests",
    ],
    mixins = [
        "x86-64",
        "linux-jammy",
        "junit-swarming-emulator",
    ],
    per_test_modifications = {
        "android_webview_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "base_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "build_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "chrome_java_test_pagecontroller_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "chrome_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "components_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "content_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "device_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "junit_unit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "keyboard_accessory_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "media_base_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "module_installer_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "net_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "paint_preview_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "password_check_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "password_manager_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "services_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "touch_to_fill_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "ui_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "webapk_client_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "webapk_shell_apk_h2o_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
        "webapk_shell_apk_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "pie-x86-emulator",
            ],
        ),
    },
)

targets.bundle(
    name = "chromium_linux_and_gl_and_vulkan_gtests",
    targets = [
        "chromium_linux_and_gl_gtests",
        "gpu_fyi_vulkan_swiftshader_gtests",
    ],
)

targets.bundle(
    name = "chromium_linux_and_gl_gtests",
    targets = [
        "chromium_linux_gtests",
        "gl_gtests_passthrough",
    ],
)

targets.bundle(
    name = "chromium_linux_cast_receiver",
    additional_compile_targets = [
        "cast_shell",
        "core_runtime_simple",
        "core_runtime_starboard",
    ],
)

targets.bundle(
    name = "chromium_linux_cast_receiver_gtests",
    targets = [
        "cast_crash_unittests",
        "cast_display_settings_unittests",
        "cast_graphics_unittests",
        "cast_receiver_gtests",
        "cast_shell_unittests",
        "cast_shell_browsertests",
        "linux_flavor_specific_chromium_gtests",
    ],
    mixins = [
        "linux-jammy",
    ],
)

targets.bundle(
    name = "chromium_linux_dbg_isolated_scripts",
    targets = [
        "desktop_chromium_isolated_scripts",
        "linux_specific_chromium_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

targets.bundle(
    name = "chromium_linux_gtests",
    targets = [
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_chromeos_only",
        "chromium_gtests_for_linux_and_mac_only",
        "chromium_gtests_for_linux_only",
        "chromium_gtests_for_win_and_linux_only",
        "linux_flavor_specific_chromium_gtests",
        "linux_specific_xr_gtests",
        "non_android_and_cast_and_chromeos_chromium_gtests",
        "non_android_chromium_gtests_no_nacl",
        "vr_platform_specific_chromium_gtests",
    ],
)

targets.bundle(
    name = "chromium_linux_rel_isolated_scripts",
    targets = [
        "chromedriver_py_tests_isolated_scripts",
        "chromium_web_tests_high_dpi_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "linux_specific_chromium_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "pytype_tests",
        "telemetry_perf_unittests_isolated_scripts",
        "vulkan_swiftshader_isolated_scripts",
    ],
)

targets.bundle(
    name = "chromium_linux_rel_isolated_scripts_code_coverage",
    targets = [
        "chromedriver_py_tests_isolated_scripts",
        "chromium_web_tests_high_dpi_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "gpu_dawn_webgpu_blink_web_tests",
        "linux_specific_chromium_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "pytype_tests",
        "telemetry_perf_unittests_isolated_scripts_xvfb",
        "vulkan_swiftshader_isolated_scripts",
    ],
)

# Like chromium_linux_rel_isolated_scripts, but should only
# include test suites that aren't affected by things like extra GN args
# (e.g. is_debug) or OS versions (e.g. Mac-12 vs Mac-13). Note: use
# chromium_linux_rel_isolated_scripts if you're setting up a new builder.
# This group should only be used across ~3 builders.
targets.bundle(
    name = "chromium_linux_rel_isolated_scripts_once",
    targets = [
        "chromedriver_py_tests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "desktop_once_isolated_scripts",
        "linux_specific_chromium_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "pytype_tests",
        "telemetry_perf_unittests_isolated_scripts",
        "vulkan_swiftshader_isolated_scripts",
        "chromium_web_tests_high_dpi_isolated_scripts",
        # TODO(crbug.com/40287410): Remove this once the BackgroundResourceFetch
        # feature launches.
        "chromium_web_tests_brfetch_isolated_scripts",
    ],
)

targets.bundle(
    name = "chromium_linux_scripts",
    targets = [
        "check_network_annotations",
        "check_static_initializers",
        "checkdeps",
        "checkperms",
        "metrics_python_tests",
        "webkit_lint",
    ],
)

targets.bundle(
    name = "chromium_mac_gtests",
    targets = [
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_mac_only",
        "mac_specific_chromium_gtests",
        "non_android_and_cast_and_chromeos_chromium_gtests",
        "non_android_chromium_gtests_no_nacl",
    ],
)

# chromium_mac_gtests_no_nacl_once in the same way.
# TODO(crbug.com/303417958): This no_nacl suite is identical to the normal
# suite, since NaCl has been disabled on Mac. Replace this by the normal
# suite.
targets.bundle(
    name = "chromium_mac_gtests_no_nacl",
    targets = [
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_mac_only",
        "mac_specific_chromium_gtests",
        "non_android_and_cast_and_chromeos_chromium_gtests",
        "non_android_chromium_gtests_no_nacl",
    ],
)

targets.bundle(
    name = "chromium_mac_osxbeta_rel_isolated_scripts",
    targets = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_mac_osxbeta_scripts",
        "mac_specific_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

targets.bundle(
    name = "chromium_mac_rel_isolated_scripts",
    targets = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "mac_specific_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

targets.bundle(
    name = "chromium_mac_rel_isolated_scripts_code_coverage",
    # TODO(crbug.com/40249801): Enable gpu_dawn_webgpu_blink_web_tests
)

# Like chromium_mac_rel_isolated_scripts above, but should only
# include test suites that aren't affected by things like extra GN args
# (e.g. is_debug) or OS versions (e.g. Mac-12 vs Mac-13). Note: use
# chromium_mac_rel_isolated_scripts if you're setting up a new builder.
targets.bundle(
    name = "chromium_mac_rel_isolated_scripts_once",
    targets = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "desktop_once_isolated_scripts",
        "mac_specific_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

targets.bundle(
    name = "chromium_mac_scripts",
    targets = [
        "check_static_initializers",
        "metrics_python_tests",
        "webkit_lint",
    ],
)

# Multiscreen tests for desktop platforms. See: crbug.com/346565331.
targets.bundle(
    name = "chromium_multiscreen_gtests",
    targets = [
        "multiscreen_interactive_ui_tests",
    ],
    per_test_modifications = {
        "multiscreen_interactive_ui_tests": targets.mixin(
            args = [
                "--gtest_filter=*MultiScreen*:*VirtualDisplayUtil*",
            ],
            swarming = targets.swarming(
                dimensions = {
                    "pool": "chromium.tests.multiscreen",
                },
            ),
        ),
    },
)

# Multiscreen tests for desktop platforms. See: crbug.com/346565331.
targets.bundle(
    name = "chromium_multiscreen_gtests_fyi",
    targets = [
        "chromium_multiscreen_gtests",
    ],
)

# On some bots we don't have capacity to run all standard tests (for example
# Android Pie), however there are tracing integration tests we want to
# ensure are still working.
targets.bundle(
    name = "chromium_tracing_gtests",
    targets = [
        "services_unittests",
    ],
)

targets.bundle(
    name = "chromium_web_tests_brfetch_isolated_scripts",
    targets = [
        "brfetch_blink_web_tests",
        "brfetch_blink_wpt_tests",
        "brfetch_headless_shell_wpt_tests",
    ],
    per_test_modifications = {
        # brfetch_blink_web_tests provides coverage for
        # running Layout Tests with BackgroundResourceFetch feature.
        "brfetch_blink_web_tests": targets.mixin(
            ci_only = True,
            swarming = targets.swarming(
                shards = 1,
            ),
            experiment_percentage = 100,
        ),
        # brfetch_blink_wpt_tests provides coverage for
        # running Layout Tests with BackgroundResourceFetch feature.
        "brfetch_blink_wpt_tests": targets.mixin(
            ci_only = True,
            swarming = targets.swarming(
                shards = 3,
            ),
            experiment_percentage = 100,
        ),
        # brfetch_headless_shell_wpt_tests provides coverage for
        # running WPTs with BackgroundResourceFetch feature.
        "brfetch_headless_shell_wpt_tests": targets.mixin(
            ci_only = True,
            swarming = targets.swarming(
                shards = 1,
            ),
            experiment_percentage = 100,
        ),
    },
)

targets.bundle(
    name = "chromium_web_tests_graphite_isolated_scripts",
    targets = [
        "graphite_enabled_blink_web_tests",
        "graphite_enabled_blink_wpt_tests",
        "graphite_enabled_headless_shell_wpt_tests",
    ],
    per_test_modifications = {
        "graphite_enabled_blink_web_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "graphite_enabled_blink_wpt_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 7,
            ),
        ),
        "graphite_enabled_headless_shell_wpt_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
    },
)

# Pixel tests only enabled on Win 10. So this is
# 'chromium_win_gtests' + 'pixel_browser_tests_gtests' +
# 'non_android_chromium_gtests_skia_gold'. When changing
# something here, also change chromium_win10_gtests_once in the same way.
targets.bundle(
    name = "chromium_win10_gtests",
    targets = [
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_win_and_linux_only",
        "fieldtrial_browser_tests",
        "non_android_and_cast_and_chromeos_chromium_gtests",
        "non_android_chromium_gtests_no_nacl",
        "non_android_chromium_gtests_skia_gold",
        "pixel_browser_tests_gtests",
        "vr_platform_specific_chromium_gtests",
        "win_specific_chromium_gtests",
    ],
)

targets.bundle(
    name = "chromium_win_dbg_isolated_scripts",
    targets = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "performance_smoke_test_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

targets.bundle(
    name = "chromium_win_gtests",
    targets = [
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_win_and_linux_only",
        "non_android_and_cast_and_chromeos_chromium_gtests",
        "non_android_chromium_gtests_no_nacl",
        "vr_platform_specific_chromium_gtests",
        "win_specific_chromium_gtests",
    ],
)

targets.bundle(
    name = "chromium_win_rel_isolated_scripts",
    targets = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "telemetry_desktop_minidump_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
        "win_specific_isolated_scripts",
    ],
)

targets.bundle(
    name = "chromium_win_rel_isolated_scripts_code_coverage",
    targets = [
        "gpu_dawn_webgpu_blink_web_tests",
    ],
)

# Like chromium_win_rel_isolated_scripts, but should only include test suites
# that aren't affected by things like extra GN args (e.g. is_debug) or OS
# versions (e.g. Mac-12 vs Mac-13). Note: use chromium_win_rel_isolated_scripts
# if you're setting up a new builder. This group should only be used across
# ~3 builders.
targets.bundle(
    name = "chromium_win_rel_isolated_scripts_once",
    targets = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "desktop_once_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "telemetry_desktop_minidump_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
        "win_specific_isolated_scripts",
    ],
)

targets.bundle(
    name = "chromium_win_scripts",
    targets = [
        "check_network_annotations",
        "metrics_python_tests",
        "webkit_lint",
    ],
)

# Compilable unit tests of cronet dependencies in:
# //components/cronet/android/dependencies.txt
# TODO(crbug.com/333888734): Add component_unittests or a subset of it.
# TODO(crbug.com/333887705): Make base_unittests compilable and add it.
# TODO(crbug.com/333888747): Make url_unittests compilable and add it.
targets.bundle(
    name = "cronet_clang_coverage_additional_gtests",
    targets = [
        "absl_hardening_tests",
        "crypto_unittests",
        "zlib_unittests",
    ],
)

# Compile targets which are common to most cronet builders in chromium.android
targets.bundle(
    name = "cronet_common_compile_targets",
    additional_compile_targets = [
        "cronet_package",
        "cronet_perf_test_apk",
        "cronet_sample_test_apk",
        "cronet_smoketests_missing_native_library_instrumentation_apk",
        "cronet_smoketests_platform_only_instrumentation_apk",
        "cronet_test_instrumentation_apk",
        "cronet_tests_android",
        "cronet_unittests_android",
        "net_unittests",
    ],
)

targets.bundle(
    name = "cronet_dbg_isolated_scripts",
    targets = [
        "cronet_sizes_suite",
    ],
)

targets.bundle(
    name = "cronet_gtests",
    targets = [
        "cronet_sample_test_apk",
        "cronet_smoketests_apk",
        "cronet_smoketests_missing_native_library_instrumentation_apk",
        "cronet_smoketests_platform_only_instrumentation_apk",
        "cronet_test_instrumentation_apk",
        "cronet_tests_android",
        "cronet_unittests_android",
        "net_unittests",
    ],
    per_test_modifications = {
        "cronet_test_instrumentation_apk": [
            targets.mixin(
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "emulator-enable-network",
        ],
        "net_unittests": targets.mixin(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
    },
)

targets.bundle(
    name = "cronet_rel_isolated_scripts",
    targets = [
        "cronet_resource_sizes",
        "cronet_sizes_suite",
    ],
)

targets.bundle(
    name = "cronet_resource_sizes",
    targets = [
        "resource_sizes_cronet_sample_apk",
    ],
    per_test_modifications = {
        "resource_sizes_cronet_sample_apk": targets.mixin(
            swarming = targets.swarming(
                # This suite simply analyzes build targets without running them.
                # It can thus run on a standard linux machine w/o a device.
                dimensions = {
                    "os": "Ubuntu-22.04",
                    "cpu": "x86-64",
                },
            ),
        ),
    },
)

targets.bundle(
    name = "cronet_sizes_suite",
    targets = [
        "cronet_sizes",
    ],
    per_test_modifications = {
        "cronet_sizes": targets.per_test_modification(
            mixins = targets.mixin(
                swarming = targets.swarming(
                    # This suite simply analyzes build targets without running them.
                    # It can thus run on a standard linux machine w/o a device.
                    dimensions = {
                        "os": "Ubuntu-22.04",
                        "cpu": "x86-64",
                    },
                ),
            ),
            remove_mixins = [
                "chromium_nexus_5x_oreo",
                "chromium_pixel_2_pie",
                "marshmallow",
                "oreo_mr1_fleet",
            ],
        ),
    },
)

targets.bundle(
    name = "desktop_chromium_mac_osxbeta_scripts",
    targets = [
        "content_shell_crash_test",
        "flatbuffers_unittests",
        "grit_python_unittests",
        "telemetry_gpu_unittests",
        "telemetry_unittests",
        "views_perftests",
    ],
    per_test_modifications = {
        "telemetry_gpu_unittests": targets.mixin(
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/549140
            ),
        ),
        "telemetry_unittests": targets.mixin(
            args = [
                "--jobs=1",
                # Disable GPU compositing, telemetry_unittests runs on VMs.
                # https://crbug.com/871955
                "--extra-browser-args=--disable-gpu",
            ],
            swarming = targets.swarming(
                shards = 8,
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "views_perftests": targets.mixin(
            args = [
                "--gtest-benchmark-name=views_perftests",
            ],
        ),
    },
)

# Script tests that only need to run on one builder per desktop platform.
targets.bundle(
    name = "desktop_once_isolated_scripts",
    targets = [
        "test_env_py_unittests",
        "xvfb_py_unittests",
    ],
)

targets.bundle(
    name = "enterprise_companion_gtests_linux",
    targets = [
        "enterprise_companion_integration_tests",
        "enterprise_companion_tests",
    ],
    per_test_modifications = {
        "enterprise_companion_integration_tests": [
            "updater-default-pool",
        ],
        "enterprise_companion_tests": [
            "updater-default-pool",
        ],
    },
)

targets.bundle(
    name = "enterprise_companion_gtests_mac",
    targets = [
        "enterprise_companion_integration_tests",
        "enterprise_companion_tests",
    ],
    per_test_modifications = {
        "enterprise_companion_integration_tests": [
            "updater-mac-pool",
        ],
        "enterprise_companion_tests": [
            "updater-mac-pool",
        ],
    },
)

targets.bundle(
    name = "enterprise_companion_gtests_win",
    targets = [
        "enterprise_companion_integration_tests",
        "enterprise_companion_tests",
    ],
    per_test_modifications = {
        "enterprise_companion_integration_tests": [
            "integrity_high",
            "updater-default-pool",
        ],
        "enterprise_companion_tests": [
            "integrity_high",
            "updater-default-pool",
        ],
    },
)

targets.bundle(
    name = "fieldtrial_android_tests",
    targets = [
        "android_browsertests_no_fieldtrial",
    ],
    per_test_modifications = {
        "android_browsertests_no_fieldtrial": targets.mixin(
            ci_only = True,
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
    },
)

targets.bundle(
    name = "fieldtrial_browser_tests",
    targets = [
        "browser_tests_no_field_trial",
        "components_browsertests_no_field_trial",
        "interactive_ui_tests_no_field_trial",
        "sync_integration_tests_no_field_trial",
    ],
    per_test_modifications = {
        "browser_tests_no_field_trial": targets.mixin(
            ci_only = True,
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "components_browsertests_no_field_trial": targets.mixin(
            ci_only = True,
        ),
        "interactive_ui_tests_no_field_trial": targets.mixin(
            ci_only = True,
        ),
        "sync_integration_tests_no_field_trial": targets.mixin(
            ci_only = True,
        ),
    },
)

targets.bundle(
    name = "fieldtrial_browser_tests_mac",
    targets = [
        "accessibility_unittests_no_field_trial",
        "browser_tests_no_field_trial",
        "components_browsertests_no_field_trial",
        "content_browsertests_no_field_trial",
        "interactive_ui_tests_no_field_trial",
        "sync_integration_tests_no_field_trial",
    ],
    per_test_modifications = {
        "accessibility_unittests_no_field_trial": targets.mixin(
            ci_only = True,
        ),
        "browser_tests_no_field_trial": targets.mixin(
            ci_only = True,
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "components_browsertests_no_field_trial": targets.mixin(
            ci_only = True,
        ),
        "content_browsertests_no_field_trial": targets.mixin(
            ci_only = True,
            swarming = targets.swarming(
                shards = 8,
            ),
        ),
        "interactive_ui_tests_no_field_trial": targets.mixin(
            ci_only = True,
        ),
        "sync_integration_tests_no_field_trial": targets.mixin(
            ci_only = True,
        ),
    },
)

targets.bundle(
    name = "fieldtrial_ios_simulator_tests",
    targets = [
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "disable_field_trial_config_for_earl_grey",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPHONE_14_17_5",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "disable_field_trial_config_for_earl_grey",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPHONE_14_17_5",
            ],
        ),
    ],
)

# Runs only the accessibility tests in CI/CQ to reduce accessibility
# failures that land.
targets.bundle(
    name = "fuchsia_accessibility_browsertests",
    targets = "accessibility_content_browsertests",
    per_test_modifications = {
        "accessibility_content_browsertests": targets.mixin(
            args = [
                "--test-arg=--disable-gpu",
                "--test-arg=--headless",
                "--test-arg=--ozone-platform=headless",
            ],
            swarming = targets.swarming(
                shards = 8,  # this may depend on runtime of a11y CQ
            ),
        ),
    },
)

targets.bundle(
    name = "fuchsia_arm64_isolated_scripts",
    targets = [
        "fuchsia_sizes_tests",
        "gpu_angle_fuchsia_unittests_isolated_scripts",
    ],
)

targets.bundle(
    name = "fuchsia_arm64_tests",
    targets = [
        "fuchsia_sizes_tests",
        targets.bundle(
            targets = [
                "gpu_validating_telemetry_tests",
                "fuchsia_gtests",
                targets.bundle(
                    targets = "gpu_angle_fuchsia_unittests_isolated_scripts",
                    mixins = "expand-as-isolated-script",
                ),
            ],
            mixins = [
                "upload_inv_extended_properties",
            ],
        ),
    ],
)

# chromium gtests running on fuchsia.
targets.bundle(
    name = "fuchsia_chrome_gtests",
    targets = [
        "absl_hardening_tests",
        "accessibility_unittests",
        "aura_unittests",
        "base_unittests",
        "blink_common_unittests",
        "blink_fuzzer_unittests",
        "blink_heap_unittests",
        "blink_platform_unittests",
        "blink_unittests",
        "boringssl_crypto_tests",
        "boringssl_ssl_tests",
        "capture_unittests",
        "cc_unittests",
        "components_browsertests",
        "components_unittests",
        "compositor_unittests",
        "content_browsertests",
        "content_unittests",
        "crypto_unittests",
        "display_unittests",
        "events_unittests",
        "filesystem_service_unittests",
        "gcm_unit_tests",
        "gfx_unittests",
        "gin_unittests",
        "google_apis_unittests",
        "gpu_unittests",
        "gwp_asan_unittests",
        "headless_browsertests",
        "headless_unittests",
        "ipc_tests",
        "latency_unittests",
        "media_unittests",
        "message_center_unittests",
        "midi_unittests",
        "mojo_unittests",
        "native_theme_unittests",
        "net_unittests",
        "ozone_gl_unittests",
        "ozone_unittests",
        "perfetto_unittests",
        # TODO(crbug.com/40274401): Enable this.
        # "rust_gtest_interop_unittests",
        "services_unittests",
        "shell_dialogs_unittests",
        "skia_unittests",
        "snapshot_unittests",
        "sql_unittests",
        "storage_unittests",
        "ui_base_unittests",
        "ui_touch_selection_unittests",
        "ui_unittests",
        "url_unittests",
        "views_examples_unittests",
        "views_unittests",
        "viz_unittests",
        "wm_unittests",
        "wtf_unittests",
        "zlib_unittests",
    ],
    per_test_modifications = {
        "cc_unittests": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "components_browsertests": targets.mixin(
            args = [
                "--test-arg=--disable-gpu",
                "--test-arg=--headless",
                "--test-arg=--ozone-platform=headless",
            ],
        ),
        "components_unittests": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "content_browsertests": targets.mixin(
            args = [
                "--test-arg=--disable-gpu",
                "--test-arg=--headless",
                "--test-arg=--ozone-platform=headless",
            ],
            swarming = targets.swarming(
                shards = 14,
            ),
        ),
        "net_unittests": targets.mixin(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.net_unittests.filter",
            ],
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
        "ozone_gl_unittests": targets.mixin(
            args = [
                "--test-arg=--ozone-platform=headless",
            ],
        ),
        "services_unittests": targets.mixin(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.services_unittests.filter",
            ],
        ),
        "ui_base_unittests": targets.mixin(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.ui_base_unittests.filter",
            ],
        ),
        "views_examples_unittests": targets.mixin(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.views_examples_unittests.filter",
            ],
        ),
        "views_unittests": targets.mixin(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.views_unittests.filter",
            ],
        ),
        "viz_unittests": targets.mixin(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.viz_unittests.filter",
            ],
        ),
    },
)

# This is a set of selected tests to test the test facility only. The
# principle of the selection includes time cost, scenario coverage,
# stability, etc; and it's subject to change. In theory, it should only be
# used by the EngProd team to verify a new test facility setup.
targets.bundle(
    name = "fuchsia_facility_gtests",
    targets = [
        "aura_unittests",
        "blink_common_unittests",
        "crypto_unittests",
        "filesystem_service_unittests",
        "web_engine_integration_tests",
        "web_engine_unittests",
    ],
    mixins = [
        "upload_inv_extended_properties",
    ],
)

# All gtests that can be run on Fuchsia CI/CQ
targets.bundle(
    name = "fuchsia_gtests",
    targets = [
        "fuchsia_chrome_gtests",
        "fuchsia_web_engine_gtests",
    ],
)

targets.bundle(
    name = "fuchsia_isolated_scripts",
    targets = [
        "chromium_webkit_isolated_scripts",
        "gpu_angle_fuchsia_unittests_isolated_scripts",
    ],
)

targets.bundle(
    name = "fuchsia_sizes_tests",
    targets = [
        "fuchsia_sizes",
    ],
)

targets.bundle(
    name = "fuchsia_standard_tests",
    targets = [
        "gpu_validating_telemetry_tests",
        "fuchsia_gtests",
        targets.bundle(
            targets = "fuchsia_isolated_scripts",
            mixins = "expand-as-isolated-script",
        ),
    ],
    mixins = [
        "upload_inv_extended_properties",
    ],
    per_test_modifications = {
        "blink_web_tests": [
            # TODO(crbug.com/337058844): uploading invocations is not supported
            # by blink_web_tests yet.
            "has_native_resultdb_integration",
        ],
        "blink_wpt_tests": [
            # TODO(crbug.com/337058844): uploading invocations is not supported
            # by blink_wpt_tests yet.
            "has_native_resultdb_integration",
        ],
        "context_lost_validating_tests": [
            # TODO(crbug.com/337058844): Merging upload_inv_extended_properties
            # with has_native_resultdb_integration is not supported yet.
            "has_native_resultdb_integration",
        ],
        "expected_color_pixel_validating_test": [
            # TODO(crbug.com/337058844): Merging upload_inv_extended_properties
            # with has_native_resultdb_integration is not supported yet.
            "has_native_resultdb_integration",
        ],
        "gpu_process_launch_tests": [
            # TODO(crbug.com/337058844): Merging upload_inv_extended_properties
            # with has_native_resultdb_integration is not supported yet.
            "has_native_resultdb_integration",
        ],
        "hardware_accelerated_feature_tests": [
            # TODO(crbug.com/337058844): Merging upload_inv_extended_properties
            # with has_native_resultdb_integration is not supported yet.
            "has_native_resultdb_integration",
        ],
        "pixel_skia_gold_validating_test": [
            # TODO(crbug.com/337058844): Merging upload_inv_extended_properties
            # with has_native_resultdb_integration is not supported yet.
            "has_native_resultdb_integration",
        ],
        "screenshot_sync_validating_tests": [
            # TODO(crbug.com/337058844): Merging upload_inv_extended_properties
            # with has_native_resultdb_integration is not supported yet.
            "has_native_resultdb_integration",
        ],
    },
)

# dedicated fuchsia gtests for web-engine and its related components.
targets.bundle(
    name = "fuchsia_web_engine_gtests",
    targets = [
        "cast_runner_browsertests",
        "cast_runner_integration_tests",
        "cast_runner_unittests",
        "web_engine_browsertests",
        "web_engine_integration_tests",
        "web_engine_unittests",
    ],
)

targets.bundle(
    name = "gl_gtests_passthrough",
    targets = [
        "gl_tests_passthrough",
        "gl_unittests",
    ],
    per_test_modifications = {
        "gl_tests_passthrough": targets.mixin(
            linux_args = [
                "--no-xvfb",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "gpu_angle_fuchsia_unittests_isolated_scripts",
    targets = [
        "angle_unittests",
    ],
)

targets.bundle(
    name = "gpu_angle_ios_end2end_gtests",
    targets = [
        "angle_end2end_tests",
    ],
    per_test_modifications = {
        "angle_end2end_tests": targets.mixin(
            args = [
                "--release",
            ],
            use_isolated_scripts_api = True,
        ),
    },
)

targets.bundle(
    name = "gpu_angle_ios_gtests",
    targets = [
        targets.bundle(
            targets = "gpu_angle_ios_end2end_gtests",
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_14_18_0",
            ],
        ),
        targets.bundle(
            targets = "gpu_angle_ios_white_box_gtests",
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_14_18_0",
            ],
        ),
    ],
)

targets.bundle(
    name = "gpu_angle_ios_white_box_gtests",
    targets = [
        "angle_white_box_tests",
    ],
    per_test_modifications = {
        "angle_white_box_tests": targets.mixin(
            args = [
                "--release",
            ],
            use_isolated_scripts_api = True,
        ),
    },
)

targets.bundle(
    name = "gpu_angle_linux_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_angle_mac_telemetry_tests",
    targets = [
        "gpu_info_collection_telemetry_tests",
        "gpu_webgl2_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl2_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_angle_unit_gtests",
    targets = [
        "angle_unittests",
    ],
    per_test_modifications = {
        "angle_unittests": targets.mixin(
            android_args = [
                "-v",
            ],
            linux_args = [
                "--no-xvfb",
            ],
            use_isolated_scripts_api = True,
        ),
    },
)

targets.bundle(
    name = "gpu_angle_win_intel_nvidia_telemetry_tests",
    targets = [
        "gpu_info_collection_telemetry_tests",
        "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
        "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_chromeos_telemetry_tests",
    targets = [
        "gpu_webgl_conformance_telemetry_tests",
    ],
)

# The command buffer perf tests are only run on Windows.
# They are mostly driver and platform independent.
targets.bundle(
    name = "gpu_command_buffer_perf_passthrough_isolated_scripts",
    targets = [
        "passthrough_command_buffer_perftests",
    ],
    per_test_modifications = {
        "passthrough_command_buffer_perftests": targets.mixin(
            args = [
                "--gtest-benchmark-name=passthrough_command_buffer_perftests",
                "-v",
                "--use-cmd-decoder=passthrough",
                "--use-angle=gl-null",
                "--fast-run",
            ],
        ),
    },
)

targets.bundle(
    name = "gpu_common_android_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_validating_telemetry_tests",
        "gpu_webgl_conformance_validating_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_common_gl_passthrough_ganesh_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_common_gtests_validating",
    targets = [
        "gl_tests_validating",
        "gl_unittests",
    ],
    per_test_modifications = {
        "gl_tests_validating": targets.mixin(
            chromeos_args = [
                "--stop-ui",
                targets.magic_args.CROS_GTEST_FILTER_FILE,
            ],
            desktop_args = [
                "--use-gpu-in-tests",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
        "gl_unittests": [
            targets.mixin(
                chromeos_args = [
                    "--stop-ui",
                    "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.gl_unittests.filter",
                ],
                desktop_args = [
                    "--use-gpu-in-tests",
                ],
                linux_args = [
                    "--no-xvfb",
                ],
            ),
            "skia_gold_test",
        ],
    },
)

targets.bundle(
    name = "gpu_common_linux_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl_conformance_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_common_metal_passthrough_graphite_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_graphite_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_common_win_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_dawn_android_compat_telemetry_tests",
    targets = [
        "gpu_dawn_webgpu_compat_cts",
        "gpu_dawn_webgpu_cts",
    ],
)

# Same as gpu_dawn_isolated_scripts, but with some suites removed:
# * telemetry_gpu_unittests since those aren't built for Android
# * SwiftShader-related tests since SwiftShader is not used on Android.
targets.bundle(
    name = "gpu_dawn_android_isolated_scripts",
    targets = [
        "gpu_dawn_perf_smoke_isolated_scripts",
        "gpu_dawn_webgpu_blink_web_tests",
    ],
)

# Same as gpu_dawn_compat_telemetry_tests, but without SwiftShader tests since
# SwiftShader is not used on Android.
targets.bundle(
    name = "gpu_dawn_android_telemetry_tests",
    targets = [
        "gpu_dawn_webgpu_cts",
    ],
)

# Same as gpu_dawn_telemetry_tests, but without SwiftShader tests since
# SwiftShader is not used on Android.
targets.bundle(
    name = "gpu_dawn_asan_isolated_scripts",
    targets = [
        "gpu_dawn_common_isolated_scripts",
        "gpu_dawn_perf_smoke_isolated_scripts",
        "gpu_dawn_webgpu_blink_web_tests",
        "gpu_dawn_webgpu_blink_web_tests_force_swiftshader",
    ],
)

targets.bundle(
    name = "gpu_dawn_common_isolated_scripts",
    targets = [
        "telemetry_gpu_unittests",
    ],
    per_test_modifications = {
        # Test that expectations files are well-formed.
        "telemetry_gpu_unittests": targets.mixin(
            swarming = targets.swarming(
                # https://crbug.com/549140
                idempotent = False,
            ),
        ),
    },
)

targets.bundle(
    name = "gpu_dawn_compat_telemetry_tests",
    targets = [
        "gpu_dawn_web_platform_webgpu_cts_force_swiftshader",
        "gpu_dawn_webgpu_compat_cts",
        "gpu_dawn_webgpu_cts",
    ],
)

targets.bundle(
    name = "gpu_dawn_gtests_no_dxc",
    targets = [
        "dawn_end2end_no_dxc_tests",
    ],
    per_test_modifications = {
        "dawn_end2end_no_dxc_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "gpu_dawn_gtests_no_dxc_with_validation",
    targets = [
        "dawn_end2end_no_dxc_validation_layers_tests",
    ],
    per_test_modifications = {
        "dawn_end2end_no_dxc_validation_layers_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "gpu_dawn_gtests_use_tint_ir",
    targets = [
        "dawn_end2end_use_tint_ir_tests",
    ],
    per_test_modifications = {
        "dawn_end2end_use_tint_ir_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
    },
)

targets.bundle(
    name = "gpu_dawn_integration_asan_gtests_passthrough",
    targets = [
        "gpu_common_gtests_passthrough",
        "gpu_dawn_gtests",
        "gpu_dawn_gtests_no_dxc",
    ],
)

# GPU gtests that test Dawn and integration with Chromium
# These tests are run both on the CI and trybots which test DEPS Dawn.
targets.bundle(
    name = "gpu_dawn_integration_gtests_passthrough",
    targets = [
        "gpu_common_gtests_passthrough",
        "gpu_dawn_gtests",
        "gpu_dawn_gtests_with_validation",
    ],
)

# TODO(crbug.com/364675466): Remove this when Tint IR is launched on macOS.
targets.bundle(
    name = "gpu_dawn_integration_gtests_passthrough_macos",
    targets = [
        "gpu_common_gtests_passthrough",
        "gpu_dawn_gtests",
        "gpu_dawn_gtests_use_tint_ir",
        "gpu_dawn_gtests_with_validation",
    ],
)

targets.bundle(
    name = "gpu_dawn_integration_gtests_passthrough_win_x64",
    targets = [
        "gpu_common_gtests_passthrough",
        "gpu_dawn_gtests",
        "gpu_dawn_gtests_no_dxc",
        "gpu_dawn_gtests_no_dxc_with_validation",
        "gpu_dawn_gtests_with_validation",
    ],
)

targets.bundle(
    name = "gpu_dawn_isolated_scripts",
    targets = [
        "gpu_dawn_common_isolated_scripts",
        "gpu_dawn_perf_smoke_isolated_scripts",
        "gpu_dawn_webgpu_blink_web_tests",
        "gpu_dawn_webgpu_blink_web_tests_force_swiftshader",
    ],
)

targets.bundle(
    name = "gpu_dawn_perf_smoke_isolated_scripts",
    targets = [
        "dawn_perf_tests",
    ],
)

targets.bundle(
    name = "gpu_dawn_telemetry_tests",
    targets = [
        "gpu_dawn_web_platform_webgpu_cts_force_swiftshader",
        "gpu_dawn_webgpu_cts",
    ],
)

targets.bundle(
    name = "gpu_dawn_telemetry_tests_fxc",
    targets = [
        "gpu_dawn_web_platform_webgpu_cts_force_swiftshader",
        "gpu_dawn_webgpu_cts_fxc",
    ],
)

targets.bundle(
    name = "gpu_dawn_telemetry_win_x64_tests",
    targets = [
        "gpu_dawn_web_platform_webgpu_cts_force_swiftshader",
        "gpu_dawn_webgpu_cts",
        "gpu_dawn_webgpu_cts_fxc",
    ],
)

targets.bundle(
    name = "gpu_dawn_tsan_gtests",
    targets = [
        "gpu_dawn_gtests",
    ],
)

targets.bundle(
    name = "gpu_dawn_web_platform_webgpu_cts_force_swiftshader",
    targets = [
        "webgpu_swiftshader_web_platform_cts_tests",
        "webgpu_swiftshader_web_platform_cts_with_validation_tests",
    ],
    per_test_modifications = {
        # We intentionally do not have worker versions of these tests since
        # non-SwiftShader coverage should be sufficient.
        "webgpu_swiftshader_web_platform_cts_tests": [
            targets.mixin(
                args = [
                    "--use-webgpu-adapter=swiftshader",
                    "--test-filter=*web_platform*",
                ],
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "gpu_integration_test_common_args",
            "webgpu_telemetry_cts",
            "linux_vulkan",
        ],
        "webgpu_swiftshader_web_platform_cts_with_validation_tests": [
            targets.mixin(
                args = [
                    "--use-webgpu-adapter=swiftshader",
                    "--test-filter=*web_platform*",
                    "--enable-dawn-backend-validation",
                ],
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "gpu_integration_test_common_args",
            "webgpu_telemetry_cts",
            "linux_vulkan",
        ],
    },
)

targets.bundle(
    name = "gpu_dawn_webgpu_blink_web_tests",
    targets = [
        "webgpu_blink_web_tests",
        "webgpu_blink_web_tests_with_backend_validation",
    ],
    per_test_modifications = {
        "webgpu_blink_web_tests": [
            targets.mixin(
                args = [
                    "--flag-specific=webgpu",
                ],
            ),
            "webgpu_cts",
        ],
        "webgpu_blink_web_tests_with_backend_validation": [
            targets.mixin(
                args = [
                    "--flag-specific=webgpu-with-backend-validation",
                    # Increase the timeout when using backend validation layers (crbug.com/1208253)
                    "--timeout-ms=30000",
                ],
            ),
            "webgpu_cts",
        ],
    },
)

targets.bundle(
    name = "gpu_dawn_webgpu_blink_web_tests_force_swiftshader",
    targets = [
        "webgpu_swiftshader_blink_web_tests",
        "webgpu_swiftshader_blink_web_tests_with_backend_validation",
    ],
    per_test_modifications = {
        "webgpu_swiftshader_blink_web_tests": [
            targets.mixin(
                args = [
                    "--flag-specific=webgpu-swiftshader",
                ],
            ),
            "webgpu_cts",
        ],
        "webgpu_swiftshader_blink_web_tests_with_backend_validation": [
            targets.mixin(
                args = [
                    "--flag-specific=webgpu-swiftshader-with-backend-validation",
                    # Increase the timeout when using backend validation layers (crbug.com/1208253)
                    "--timeout-ms=30000",
                ],
            ),
            "webgpu_cts",
        ],
    },
)

targets.bundle(
    name = "gpu_dawn_webgpu_compat_cts",
    targets = [
        "webgpu_cts_compat_tests",
    ],
    per_test_modifications = {
        # Worker versions of compat tests intentionally omitted since it is
        # unlikely that the compat path will interact with workers.
        "webgpu_cts_compat_tests": [
            targets.mixin(
                args = [
                    "--extra-browser-args=--enable-features=WebGPUExperimentalFeatures --use-webgpu-adapter=opengles",
                ],
                android_args = [
                    "--extra-browser-args=--use-angle=gles --disable-features=Vulkan",
                ],
                linux_args = [
                    "--extra-browser-args=--use-angle=gl",
                ],
                swarming = targets.swarming(
                    shards = 14,
                ),
                android_swarming = targets.swarming(
                    shards = 36,
                ),
            ),
            "gpu_integration_test_common_args",
            "webgpu_telemetry_cts",
        ],
    },
)

targets.bundle(
    name = "gpu_dawn_webgpu_cts_asan",
    # We intentionally do not have fxc + worker tests since dxc + worker
    # should provide sufficient coverage.
    targets = [
        "webgpu_cts_dedicated_worker_tests",
        "webgpu_cts_fxc_tests",
        "webgpu_cts_service_worker_tests",
        "webgpu_cts_shared_worker_tests",
        "webgpu_cts_tests",
    ],
    per_test_modifications = {
        "webgpu_cts_dedicated_worker_tests": [
            targets.mixin(
                swarming = targets.swarming(
                    shards = 1,
                ),
            ),
            "gpu_integration_test_common_args",
            "webgpu_telemetry_cts",
            "linux_vulkan",
        ],
        "webgpu_cts_fxc_tests": [
            targets.mixin(
                args = [
                    "--use-fxc",
                ],
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "gpu_integration_test_common_args",
            "webgpu_telemetry_cts",
            "linux_vulkan",
        ],
        "webgpu_cts_service_worker_tests": [
            targets.mixin(
                swarming = targets.swarming(
                    shards = 1,
                ),
            ),
            "gpu_integration_test_common_args",
            "webgpu_telemetry_cts",
            "linux_vulkan",
        ],
        "webgpu_cts_shared_worker_tests": [
            targets.mixin(
                swarming = targets.swarming(
                    shards = 1,
                ),
            ),
            "gpu_integration_test_common_args",
            "webgpu_telemetry_cts",
            "linux_vulkan",
        ],
        "webgpu_cts_tests": [
            targets.mixin(
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "gpu_integration_test_common_args",
            "webgpu_telemetry_cts",
            "linux_vulkan",
        ],
    },
)

targets.bundle(
    name = "gpu_dawn_webgpu_cts_fxc",
    # We intentionally do not have fxc + worker tests since dxc + worker
    # should provide sufficient coverage.
    targets = [
        "webgpu_cts_fxc_tests",
        "webgpu_cts_fxc_with_validation_tests",
    ],
    per_test_modifications = {
        "webgpu_cts_fxc_tests": [
            targets.mixin(
                args = [
                    "--use-fxc",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 14,
                ),
            ),
            "gpu_integration_test_common_args",
            "webgpu_telemetry_cts",
            "linux_vulkan",
        ],
        "webgpu_cts_fxc_with_validation_tests": [
            targets.mixin(
                args = [
                    "--enable-dawn-backend-validation",
                    "--use-fxc",
                ],
                ci_only = True,
                swarming = targets.swarming(
                    shards = 14,
                ),
            ),
            "gpu_integration_test_common_args",
            "webgpu_telemetry_cts",
            "linux_vulkan",
        ],
    },
)

targets.bundle(
    name = "gpu_default_and_optional_win_media_foundation_specific_gtests",
    targets = [
        # MediaFoundation browser tests, which currently only run on Windows OS,
        # and require physical hardware.
        "media_foundation_browser_tests",
    ],
    per_test_modifications = {
        "media_foundation_browser_tests": targets.mixin(
            args = [
                "--use-gpu-in-tests",
            ],
        ),
    },
)

targets.bundle(
    name = "gpu_default_and_optional_win_specific_gtests",
    targets = [
        "xr_browser_tests",
    ],
    per_test_modifications = {
        "xr_browser_tests": targets.mixin(
            args = [
                # The Windows machines this is run on should always meet all the
                # requirements, so skip the runtime checks to help catch issues, e.g.
                # if we're incorrectly being told a DirectX 11.1 device isn't
                # available
                "--ignore-runtime-requirements=*",
            ],
        ),
    },
)

targets.bundle(
    name = "gpu_desktop_passthrough_gtests",
    targets = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_desktop_specific_gtests",
    ],
)

targets.bundle(
    name = "gpu_desktop_specific_gtests",
    targets = [
        "tab_capture_end2end_tests",
    ],
    per_test_modifications = {
        "tab_capture_end2end_tests": targets.mixin(
            args = [
                "--enable-gpu",
                "--test-launcher-bot-mode",
                "--test-launcher-jobs=1",
                "--gtest_filter=TabCaptureApiPixelTest.EndToEnd*",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
    },
)

targets.bundle(
    name = "gpu_fyi_and_optional_non_linux_gtests",
    targets = [
        # gpu_unittests is killing the Swarmed Linux GPU bots similarly to
        # how content_unittests was: http://crbug.com/763498 .
        "gpu_unittests",
    ],
)

targets.bundle(
    name = "gpu_fyi_and_optional_win_specific_gtests",
    targets = [
        # WebNN DirectML backend unit tests, which currently only run on
        # Windows OS, and require physical hardware.
        "services_webnn_unittests",
    ],
    per_test_modifications = {
        "services_webnn_unittests": targets.mixin(
            args = [
                "--use-gpu-in-tests",
            ],
        ),
    },
)

targets.bundle(
    name = "gpu_fyi_android_gtests",
    targets = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_common_gtests_validating",
        "gpu_fyi_and_optional_non_linux_gtests",
    ],
)

targets.bundle(
    name = "gpu_fyi_android_shieldtv_gtests",
    targets = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_common_gtests_validating",
        "gpu_fyi_and_optional_non_linux_gtests",
    ],
)

targets.bundle(
    name = "gpu_fyi_android_webgl2_and_gold_telemetry_tests",
    targets = [
        "gpu_validating_telemetry_tests",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_validating_telemetry_tests",
    ],
)

# TODO(crbug.com/40130073): Merge with an existing set of tests such as
# gpu_fyi_linux_release_gtests once all CrOS tests have been enabled.
targets.bundle(
    name = "gpu_fyi_chromeos_release_gtests",
    targets = [
        "gpu_common_gtests_passthrough",
    ],
)

targets.bundle(
    name = "gpu_fyi_chromeos_release_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_fyi_lacros_release_gtests",
    targets = [
        "gpu_memory_buffer_impl_tests_suite",
    ],
)

# The same as gpu_fyi_chromeos_release_telemetry_tests, but using
# passthrough instead of validating since the Lacros bots are actually
# Lacros-like Linux bots, and Linux uses the passthrough decoder.
# Additionally, we use GLES instead of GL since that's what is supported.
targets.bundle(
    name = "gpu_fyi_lacros_release_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_fyi_linux_debug_gtests",
    targets = [
        "gpu_common_gtests_passthrough",
    ],
)

targets.bundle(
    name = "gpu_fyi_linux_debug_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_fyi_linux_release_gtests",
    targets = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_desktop_specific_gtests",
        "gpu_memory_buffer_impl_tests_suite",
        "gpu_vulkan_gtests",
    ],
)

targets.bundle(
    name = "gpu_fyi_linux_release_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_fyi_mac_debug_gtests",
    targets = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_fyi_and_optional_non_linux_gtests",
        "gpu_fyi_mac_specific_gtests",
    ],
)

targets.bundle(
    name = "gpu_fyi_mac_nvidia_release_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webcodecs_gl_passthrough_ganesh_telemetry_test",
        "gpu_webgl2_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_fyi_mac_pro_release_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl2_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_graphite_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_fyi_mac_release_gtests",
    targets = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_desktop_specific_gtests",
        "gpu_fyi_and_optional_non_linux_gtests",
        "gpu_fyi_mac_specific_gtests",
    ],
)

targets.bundle(
    name = "gpu_fyi_mac_release_telemetry_tests",
    targets = [
        "gpu_gl_passthrough_ganesh_telemetry_tests",
        "gpu_metal_passthrough_ganesh_telemetry_tests",
        "gpu_webcodecs_gl_passthrough_ganesh_telemetry_test",
        "gpu_webcodecs_metal_passthrough_ganesh_telemetry_test",
        "gpu_webcodecs_metal_passthrough_graphite_telemetry_test",
        "gpu_webgl2_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl2_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_fyi_mac_specific_gtests",
    targets = [
        # Face and barcode detection unit tests, which currently only run on
        # Mac OS, and require physical hardware.
        "services_unittests",
    ],
    per_test_modifications = {
        "services_unittests": targets.mixin(
            args = [
                "--gtest_filter=*Detection*",
                "--use-gpu-in-tests",
            ],
        ),
    },
)

targets.bundle(
    name = "gpu_fyi_vulkan_swiftshader_gtests",
    targets = [
        "vulkan_swiftshader_content_browsertests",
    ],
    per_test_modifications = {
        "vulkan_swiftshader_content_browsertests": targets.mixin(
            args = [
                "--enable-gpu",
                "--test-launcher-bot-mode",
                "--test-launcher-jobs=1",
                "--test-launcher-filter-file=../../testing/buildbot/filters/vulkan.content_browsertests.filter",
                "--enable-features=UiGpuRasterization,Vulkan",
                "--use-vulkan=swiftshader",
                "--enable-gpu-rasterization",
                "--disable-software-compositing-fallback",
                "--disable-vulkan-fallback-to-gl-for-testing",
                "--disable-headless-mode",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
    },
)

targets.bundle(
    name = "gpu_fyi_win_amd_release_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_fyi_win_debug_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
        "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_fyi_win_gtests",
    targets = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_default_and_optional_win_media_foundation_specific_gtests",
        "gpu_default_and_optional_win_specific_gtests",
        "gpu_desktop_specific_gtests",
        "gpu_fyi_and_optional_non_linux_gtests",
        "gpu_fyi_and_optional_win_specific_gtests",
    ],
)

targets.bundle(
    name = "gpu_fyi_win_intel_release_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
        "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_fyi_win_optional_isolated_scripts",
    targets = [
        "gpu_command_buffer_perf_passthrough_isolated_scripts",
    ],
)

targets.bundle(
    name = "gpu_info_collection_telemetry_tests",
    targets = [
        "info_collection_tests",
    ],
    per_test_modifications = {
        "info_collection_tests": [
            targets.mixin(
                args = [
                    targets.magic_args.GPU_EXPECTED_VENDOR_ID,
                    targets.magic_args.GPU_EXPECTED_DEVICE_ID,
                    "--extra-browser-args=--force_high_performance_gpu",
                ],
            ),
            "gpu_integration_test_common_args",
        ],
    },
)

targets.bundle(
    name = "gpu_memory_buffer_impl_tests_suite",
    targets = [
        "gpu_memory_buffer_impl_tests",
    ],
    per_test_modifications = {
        "gpu_memory_buffer_impl_tests": targets.mixin(
            args = [
                "--enable-gpu",
                "--use-gpu-in-tests",
                "--gtest_filter=*GpuMemoryBufferImplTest*",
            ],
            lacros_args = [
                "--ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
    },
)

targets.bundle(
    name = "gpu_nexus5x_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_validating_telemetry_tests",
        "gpu_webcodecs_validating_telemetry_test",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_validating_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_pixel_4_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_validating_telemetry_tests",
        "gpu_webcodecs_validating_telemetry_test",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_validating_telemetry_tests",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_validating_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_pixel_6_telemetry_tests",
    targets = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_graphite_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_validating_telemetry_tests",
        "gpu_webcodecs_validating_graphite_telemetry_test",
        "gpu_webcodecs_validating_telemetry_test",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_validating_telemetry_tests",
        "gpu_webgl_conformance_gles_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_validating_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_swangle_telemetry_tests",
    targets = [
        "gpu_webgl_conformance_swangle_passthrough_telemetry_tests",
    ],
)

targets.bundle(
    name = "gpu_vulkan_gtests",
    targets = [
        "vulkan_tests",
    ],
    per_test_modifications = {
        "vulkan_tests": targets.mixin(
            desktop_args = [
                "--use-gpu-in-tests",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
    },
)

targets.bundle(
    name = "gpu_webcodecs_validating_graphite_telemetry_test",
    targets = [
        "webcodecs_graphite_tests",
    ],
    per_test_modifications = {
        "webcodecs_graphite_tests": [
            targets.mixin(
                args = [
                    "--extra-browser-args=--use-cmd-decoder=validating --enable-features=SkiaGraphite",
                ],
            ),
            "gpu_integration_test_common_args",
        ],
    },
)

targets.bundle(
    name = "gpu_webcodecs_validating_telemetry_test",
    targets = [
        "webcodecs_tests",
    ],
    per_test_modifications = {
        "webcodecs_tests": [
            targets.mixin(
                args = [
                    "--extra-browser-args=--use-cmd-decoder=validating",
                ],
            ),
            "gpu_integration_test_common_args",
        ],
    },
)

targets.bundle(
    name = "gpu_webgl2_conformance_validating_telemetry_tests",
    targets = [
        "webgl2_conformance_validating_tests",
    ],
    per_test_modifications = {
        "webgl2_conformance_validating_tests": [
            targets.mixin(
                args = [
                    "--webgl-conformance-version=2.0.1",
                    targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
                    # On dual-GPU devices we want the high-performance GPU to be active
                    "--extra-browser-args=--use-cmd-decoder=validating --force_high_performance_gpu",
                ],
                swarming = targets.swarming(
                    # These tests currently take about an hour and fifteen minutes
                    # to run. Split them into roughly 5-minute shards.
                    shards = 20,
                ),
            ),
            "gpu_integration_test_common_args",
        ],
    },
)

targets.bundle(
    name = "gpu_webgl_conformance_gles_passthrough_graphite_telemetry_tests",
    targets = [
        "webgl_conformance_gles_passthrough_graphite_tests",
    ],
    per_test_modifications = {
        "webgl_conformance_gles_passthrough_graphite_tests": [
            targets.mixin(
                args = [
                    "--extra-browser-args=--use-gl=angle --use-angle=gles --use-cmd-decoder=passthrough --force_high_performance_gpu --enable-features=SkiaGraphite",
                    targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
                ],
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "gpu_integration_test_common_args",
        ],
    },
)

targets.bundle(
    name = "gpu_webgl_conformance_swangle_passthrough_telemetry_tests",
    targets = [
        "webgl_conformance_swangle_passthrough_tests",
    ],
    per_test_modifications = {
        "webgl_conformance_swangle_passthrough_tests": [
            targets.mixin(
                args = [
                    "--extra-browser-args=--use-gl=angle --use-angle=swiftshader --use-cmd-decoder=passthrough",
                    "--xvfb",
                ],
                swarming = targets.swarming(
                    shards = 1,
                ),
            ),
            "gpu_integration_test_common_args",
        ],
    },
)

targets.bundle(
    name = "gpu_webgl_conformance_telemetry_tests",
    targets = [
        "webgl_conformance_tests",
    ],
    per_test_modifications = {
        "webgl_conformance_tests": [
            targets.mixin(
                args = [
                    # On dual-GPU devices we want the high-performance GPU to be active
                    "--extra-browser-args=--force_high_performance_gpu",
                    targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
                ],
                swarming = targets.swarming(
                    shards = 2,
                ),
                android_swarming = targets.swarming(
                    shards = 12,
                ),
                chromeos_swarming = targets.swarming(
                    shards = 20,
                ),
            ),
            "gpu_integration_test_common_args",
        ],
    },
)

targets.bundle(
    name = "gpu_win_gtests",
    targets = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_default_and_optional_win_specific_gtests",
        "gpu_desktop_specific_gtests",
    ],
)

targets.bundle(
    name = "headless_browser_gtests",
    targets = [
        "headless_browsertests",
        "headless_unittests",
    ],
)

targets.bundle(
    name = "headless_shell_wpt_tests_isolated_scripts",
    targets = [
        "headless_shell_wpt_tests_include_all",
    ],
    per_test_modifications = {
        "headless_shell_wpt_tests_include_all": targets.mixin(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
    },
)

targets.bundle(
    name = "ios17_beta_simulator_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_18_2",
                "SIM_IPHONE_14_18_2",
            ],
        ),
        targets.bundle(
            targets = "ios_crash_xcuitests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_18_2",
                "SIM_IPHONE_14_18_2",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_18_2",
                "SIM_IPHONE_14_18_2",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_18_2",
                "SIM_IPHONE_14_18_2",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_18_2",
                "SIM_IPHONE_14_18_2",
                "SIM_IPHONE_SE_3RD_GEN_18_2",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios17_sdk_simulator_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_18_2",
                "SIM_IPHONE_14_18_2",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_18_2",
                "SIM_IPHONE_14_18_2",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_18_2",
                "SIM_IPHONE_14_18_2",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_18_2",
                "SIM_IPHONE_14_18_2",
                "SIM_IPHONE_SE_3RD_GEN_18_2",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios18_beta_simulator_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_crash_xcuitests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPAD_10TH_GEN_18_1",
                "SIM_IPAD_AIR_6TH_GEN_18_1",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPAD_10TH_GEN_18_1",
                "SIM_IPAD_AIR_6TH_GEN_18_1",
                "SIM_IPAD_PRO_7TH_GEN_18_1",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_AIR_6TH_GEN_18_1",
                "SIM_IPAD_PRO_7TH_GEN_18_1",
                "SIM_IPHONE_15_18_1",
                "SIM_IPHONE_15_PRO_MAX_18_1",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios18_sdk_simulator_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_crash_xcuitests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPAD_PRO_7TH_GEN_18_1",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPAD_PRO_7TH_GEN_18_1",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_AIR_6TH_GEN_18_1",
                "SIM_IPAD_PRO_7TH_GEN_18_1",
                "SIM_IPHONE_15_18_1",
                "SIM_IPHONE_SE_3RD_GEN_18_1",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios_asan_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios_blink_dbg_tests",
    targets = [
        targets.bundle(
            targets = "ios_blink_tests",
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios_blink_tests",
    targets = [
        "absl_hardening_tests",
        "angle_unittests",
        "base_unittests",
        "blink_common_unittests",
        "blink_fuzzer_unittests",
        "blink_heap_unittests",
        "blink_platform_unittests",
        "boringssl_crypto_tests",
        "boringssl_ssl_tests",
        "capture_unittests",
        "cast_unittests",
        "cc_unittests",
        "components_browsertests",
        "components_unittests",
        "compositor_unittests",
        "content_browsertests",
        "content_unittests",
        "crashpad_tests",
        "crypto_unittests",
        "device_unittests",
        "display_unittests",
        "env_chromium_unittests",
        "events_unittests",
        "gcm_unit_tests",
        "gfx_unittests",
        "gin_unittests",
        "gl_unittests",
        "google_apis_unittests",
        "gpu_unittests",
        "gwp_asan_unittests",
        "latency_unittests",
        "leveldb_unittests",
        "libjingle_xmpp_unittests",
        "liburlpattern_unittests",
        "media_unittests",
        "media_unittests_skia_graphite_dawn",
        "media_unittests_skia_graphite_metal",
        "midi_unittests",
        "mojo_unittests",
        "native_theme_unittests",
        "net_unittests",
        "perfetto_unittests",
        "printing_unittests",
        "sandbox_unittests",
        "services_unittests",
        "shell_dialogs_unittests",
        "skia_unittests",
        "sql_unittests",
        "storage_unittests",
        "ui_base_unittests",
        "ui_touch_selection_unittests",
        "ui_unittests",
        "url_unittests",
        "viz_unittests",
        "wtf_unittests",
        "zlib_unittests",
    ],
    per_test_modifications = {
        "angle_unittests": targets.mixin(
            use_isolated_scripts_api = True,
        ),
        "base_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.base_unittests.filter",
            ],
        ),
        "blink_platform_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.blink_platform_unittests.filter",
            ],
        ),
        "cc_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.cc_unittests.filter",
                "--use-gpu-in-tests",
            ],
        ),
        "components_browsertests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.use_blink.components_browsertests.filter",
            ],
        ),
        "components_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.use_blink.components_unittests.filter",
            ],
        ),
        "compositor_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.compositor_unittests.filter",
            ],
        ),
        "content_browsertests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.content_browsertests.filter",
            ],
        ),
        "content_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.content_unittests.filter",
            ],
        ),
        "gfx_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.gfx_unittests.filter",
            ],
        ),
        "gpu_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.gpu_unittests.filter",
            ],
        ),
        "media_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.media_unittests.filter",
            ],
        ),
        "media_unittests_skia_graphite_dawn": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.media_unittests.filter",
            ],
        ),
        "media_unittests_skia_graphite_metal": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.media_unittests.filter",
            ],
        ),
        "mojo_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.mojo_unittests.filter",
            ],
        ),
        "ui_base_unittests": targets.mixin(
            args = [
                "--test-launcher-filter-file=testing/buildbot/filters/ios.ui_base_unittests.filter",
            ],
        ),
        "viz_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.viz_unittests.filter",
                "--use-gpu-in-tests",
            ],
        ),
    },
)

targets.bundle(
    name = "ios_clang_tot_device_tests",
    targets = [
        targets.bundle(
            targets = "clang_tot_gtests",
            variants = [
                "IPHONE_15_PRO_18_0",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios_clang_tot_sim_tests",
    targets = [
        targets.bundle(
            targets = "clang_tot_gtests",
            variants = [
                "SIM_IPHONE_X_16_4",
            ],
        ),
    ],
)

# This suite is a union of ios_simulator_tests and
# ios_simulator_full_configs_tests.
targets.bundle(
    name = "ios_code_coverage_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPHONE_14_16_4",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_16_4",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPAD_PRO_6TH_GEN_16_4",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
                "SIM_IPHONE_14_16_4",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_PRO_6TH_GEN_16_4",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
                "SIM_IPHONE_14_16_4",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_PRO_6TH_GEN_16_4",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
                "SIM_IPHONE_14_16_4",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios_common_tests",
    targets = [
        "absl_hardening_tests",
        "boringssl_crypto_tests",
        "boringssl_ssl_tests",
        "crashpad_tests",
        "crypto_unittests",
        "google_apis_unittests",
        "ios_components_unittests",
        "ios_net_unittests",
        "ios_testing_unittests",
        "net_unittests",
        # TODO(https://bugs.chromium.org/p/gn/issues/detail?id=340): Enable this.
        # "rust_gtest_interop_unittests",
        "services_unittests",
        "sql_unittests",
        "url_unittests",
    ],
    per_test_modifications = {
        "ios_net_unittests": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
    },
)

targets.bundle(
    name = "ios_crash_xcuitests",
    targets = [
        "ios_crash_xcuitests_module",
    ],
)

targets.bundle(
    name = "ios_eg2_cq_tests",
    targets = [
        "ios_chrome_integration_eg2tests_module",
        "ios_web_shell_eg2tests_module",
    ],
    per_test_modifications = {
        "ios_chrome_integration_eg2tests_module": [
            targets.mixin(
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "ios_parallel_simulators",
        ],
    },
)

targets.bundle(
    name = "ios_eg2_tests",
    targets = [
        "ios_chrome_bookmarks_eg2tests_module",
        "ios_chrome_settings_eg2tests_module",
        "ios_chrome_signin_eg2tests_module",
        "ios_chrome_smoke_eg2tests_module",
        "ios_chrome_ui_eg2tests_module",
        "ios_chrome_web_eg2tests_module",
    ],
    per_test_modifications = {
        "ios_chrome_bookmarks_eg2tests_module": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "ios_chrome_settings_eg2tests_module": [
            targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "ios_parallel_simulators",
        ],
        "ios_chrome_signin_eg2tests_module": targets.mixin(
            swarming = targets.swarming(
                shards = 6,
            ),
        ),
        "ios_chrome_ui_eg2tests_module": [
            targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "ios_parallel_simulators",
        ],
        "ios_chrome_web_eg2tests_module": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "ios_m1_simulator_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_1",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_1",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_1",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_14_PRO_MAX_17_5",
                "SIM_IPHONE_15_18_1",
                "SIM_IPHONE_15_PRO_MAX_18_1",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios_screen_size_dependent_tests",
    targets = [
        "base_unittests",
        "components_unittests",
        "gfx_unittests",
        "ios_chrome_unittests",
        "ios_web_inttests",
        "ios_web_unittests",
        "ios_web_view_inttests",
        "ios_web_view_unittests",
        "skia_unittests",
        "ui_base_unittests",
    ],
)

# Please also change ios_code_coverage_tests for any change in this suite.
targets.bundle(
    name = "ios_simulator_full_configs_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPHONE_14_PLUS_17_5",
                "SIM_IPHONE_14_PLUS_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_1",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_1",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_1",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_1",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios_simulator_noncq_tests",
    targets = [
        targets.bundle(
            targets = "ios_crash_xcuitests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_SE_3RD_GEN_16_4",
                "SIM_IPHONE_SE_3RD_GEN_17_5",
                "SIM_IPHONE_SE_3RD_GEN_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_16_4",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPAD_PRO_6TH_GEN_16_4",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_16_4",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_1",
                "SIM_IPAD_PRO_6TH_GEN_16_4",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_1",
                "SIM_IPHONE_14_PLUS_16_4",
                "SIM_IPHONE_14_PLUS_17_5",
                "SIM_IPHONE_14_PLUS_18_1",
                "SIM_IPHONE_SE_3RD_GEN_16_4",
                "SIM_IPHONE_SE_3RD_GEN_17_5",
                "SIM_IPHONE_SE_3RD_GEN_18_1",
            ],
        ),
    ],
)

# Please also change ios_code_coverage_tests for any change in this suite.
targets.bundle(
    name = "ios_simulator_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_1",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPHONE_15_18_1",
                "SIM_IPAD_PRO_7TH_GEN_18_1",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios_vm_eg2_tests",
    targets = [
        "ios_chrome_smoke_eg2tests_module",
    ],
)

targets.bundle(
    name = "ios_vm_tests",
    targets = [
        targets.bundle(
            targets = "ios_vm_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_10TH_GEN_17_5",
                "SIM_IPAD_10TH_GEN_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_14_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_vm_unittests",
            variants = [
                "SIM_IPAD_10TH_GEN_17_5",
                "SIM_IPAD_10TH_GEN_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_14_18_0",
            ],
        ),
    ],
)

targets.bundle(
    name = "ios_vm_unittests",
    targets = [
        "ios_chrome_unittests",
    ],
)

targets.bundle(
    name = "ios_webkit_tot_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
    ],
)

targets.bundle(
    name = "js_code_coverage_browser_tests_suite",
    targets = [
        "js_code_coverage_browser_tests",
    ],
    per_test_modifications = {
        "js_code_coverage_browser_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 16,
            ),
        ),
    },
)

targets.bundle(
    name = "leak_detection_isolated_scripts",
    targets = [
        "memory.leak_detection",
    ],
    per_test_modifications = {
        "memory.leak_detection": targets.mixin(
            swarming = targets.swarming(
                shards = 10,
                expiration_sec = 36000,
                hard_timeout_sec = 10800,
                io_timeout_sec = 3600,
            ),
        ),
    },
)

targets.bundle(
    name = "linux_cfm_gtests",
    targets = [
        "chromeos_unittests",
        "unit_tests",
    ],
)

targets.bundle(
    name = "linux_chromeos_gtests_oobe",
    targets = [
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_chromeos_only",
        "chromium_gtests_for_win_and_linux_only",
        "linux_chromeos_lacros_gtests",
        "linux_chromeos_oobe_specific_tests",
        "linux_chromeos_specific_gtests",
        "linux_flavor_specific_chromium_gtests",
        "non_android_chromium_gtests",
    ],
)

targets.bundle(
    name = "linux_chromeos_oobe_specific_tests",
    targets = [
        # TODO(crbug.com/40126889): Merge this suite back in to the main
        # browser_tests when the tests no longer fail on MSAN.
        "oobe_only_browser_tests",
    ],
    per_test_modifications = {
        "oobe_only_browser_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 20,
            ),
            experiment_percentage = 100,
        ),
    },
)

# This is for linux-chromeos-rel CQ builder.
targets.bundle(
    name = "linux_chromeos_rel_cq",
    targets = [
        "ash_pixel_gtests",
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_chromeos_only",
        "chromium_gtests_for_win_and_linux_only",
        "linux_chromeos_lacros_gtests",
        "linux_chromeos_specific_gtests",
        "linux_flavor_specific_chromium_gtests",
        "non_android_chromium_gtests",
        "pixel_experimental_browser_tests_gtests",
    ],
)

targets.bundle(
    name = "linux_force_accessibility_gtests",
    targets = [
        "browser_tests",
        "content_browsertests",
        "interactive_ui_tests",
    ],
    per_test_modifications = {
        "browser_tests": targets.mixin(
            args = [
                "--force-renderer-accessibility",
                "--test-launcher-filter-file=../../testing/buildbot/filters/accessibility-linux.browser_tests.filter",
            ],
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "content_browsertests": targets.mixin(
            args = [
                "--force-renderer-accessibility",
                "--test-launcher-filter-file=../../testing/buildbot/filters/accessibility-linux.content_browsertests.filter",
            ],
            swarming = targets.swarming(
                shards = 8,
            ),
        ),
        "interactive_ui_tests": targets.mixin(
            args = [
                "--force-renderer-accessibility",
                "--test-launcher-filter-file=../../testing/buildbot/filters/accessibility-linux.interactive_ui_tests.filter",
            ],
            swarming = targets.swarming(
                shards = 6,
            ),
        ),
    },
)

targets.bundle(
    name = "linux_optional_gpu_tests_rel_gpu_telemetry_tests",
    targets = [
        targets.bundle(
            targets = "gpu_common_and_optional_telemetry_tests",
            variants = [
                "LINUX_INTEL_UHD_630_STABLE",
                "LINUX_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webcodecs_telemetry_test",
            variants = [
                "LINUX_INTEL_UHD_630_STABLE",
                "LINUX_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
            variants = [
                "LINUX_INTEL_UHD_630_STABLE",
                "LINUX_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
            variants = [
                "LINUX_INTEL_UHD_630_STABLE",
                "LINUX_NVIDIA_GTX_1660_STABLE",
            ],
        ),
    ],
)

targets.bundle(
    name = "linux_specific_xr_gtests",
    targets = [
        "xr_browser_tests",
    ],
)

targets.bundle(
    name = "linux_viz_gtests",
    targets = [
        "gpu_fyi_vulkan_swiftshader_gtests",
    ],
)

# TODO(crbug.com/40223516): Remove this set of test suites when LSan can be
# enabled Mac ASan bots. This list will be gradually filled with more tests
# until the bot has parity with ASan bots, and the ASan bot can then enable
# LSan and the mac-lsan-fyi-rel bot go away.
targets.bundle(
    name = "mac_lsan_fyi_gtests",
    targets = [
        "absl_hardening_tests",
        "accessibility_unittests",
        "app_shell_unittests",
        "base_unittests",
        "blink_heap_unittests",
        "blink_platform_unittests",
        "blink_unittests",
        "cc_unittests",
        "components_unittests",
        "content_unittests",
        "crashpad_tests",
        "cronet_unittests",
        "device_unittests",
        "net_unittests",
        # TODO(crbug.com/40274401): Enable this.
        # "rust_gtest_interop_unittests",
    ],
)

targets.bundle(
    name = "mac_optional_gpu_tests_rel_gpu_telemetry_tests",
    targets = [
        targets.bundle(
            targets = "gpu_common_and_optional_telemetry_tests",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
                "MAC_RETINA_NVIDIA_GPU_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_gl_passthrough_ganesh_telemetry_tests",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_metal_passthrough_ganesh_telemetry_tests",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webcodecs_gl_passthrough_ganesh_telemetry_test",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
                "MAC_RETINA_NVIDIA_GPU_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webcodecs_metal_passthrough_ganesh_telemetry_test",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webcodecs_metal_passthrough_graphite_telemetry_test",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webgl2_conformance_metal_passthrough_graphite_telemetry_tests",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webgl_conformance_metal_passthrough_ganesh_telemetry_tests",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
    ],
)

targets.bundle(
    name = "mac_optional_gpu_tests_rel_gtests",
    targets = [
        targets.bundle(
            targets = "gpu_fyi_and_optional_non_linux_gtests",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
                "MAC_RETINA_NVIDIA_GPU_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_fyi_mac_specific_gtests",
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
                "MAC_RETINA_NVIDIA_GPU_STABLE",
            ],
        ),
    ],
)

targets.bundle(
    name = "mac_specific_chromium_gtests",
    targets = [
        "power_sampler_unittests",
        "sandbox_unittests",
        "updater_tests",
        "xr_browser_tests",
    ],
)

targets.bundle(
    name = "monochrome_public_apk_checker_isolated_script",
    targets = [
        "monochrome_public_apk_checker",
    ],
    per_test_modifications = {
        "monochrome_public_apk_checker": targets.per_test_modification(
            mixins = targets.mixin(
                swarming = targets.swarming(
                    dimensions = {
                        "os": "Ubuntu-22.04",
                        "cpu": "x86-64",
                        "device_os": None,
                        "device_os_flavor": None,
                        "device_playstore_version": None,
                        "device_type": None,
                    },
                ),
            ),
            remove_mixins = [
                "chromium_nexus_5x_oreo",
                "chromium_pixel_2_pie",
                "marshmallow",
                "oreo_mr1_fleet",
            ],
        ),
    },
)

targets.bundle(
    name = "network_service_extra_gtests",
    targets = [
        "network_service_fyi_gtests",
    ],
)

targets.bundle(
    name = "network_service_fyi_gtests",
    targets = [
        "network_service_web_request_proxy_browser_tests",
    ],
    per_test_modifications = {
        "network_service_web_request_proxy_browser_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 15,
            ),
        ),
    },
)

targets.bundle(
    name = "non_android_and_cast_and_chromeos_chromium_gtests",
    targets = [
        "cronet_tests",
        "cronet_unittests",
        "headless_browsertests",
        "headless_unittests",
    ],
)

targets.bundle(
    name = "non_android_chromium_gtests_no_nacl",
    targets = [
        "accessibility_unittests",
        "app_shell_unittests",
        "blink_fuzzer_unittests",
        "browser_tests",
        "chrome_app_unittests",
        "chromedriver_unittests",
        "extensions_browsertests",
        "extensions_unittests",
        "filesystem_service_unittests",
        "interactive_ui_tests",
        "message_center_unittests",
        "native_theme_unittests",
        "pdf_unittests",
        "printing_unittests",
        "remoting_unittests",
        "snapshot_unittests",
        "sync_integration_tests",
        "ui_unittests",
        "views_unittests",
    ],
    per_test_modifications = {
        "browser_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "interactive_ui_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "sync_integration_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
    },
)

targets.bundle(
    name = "non_android_chromium_gtests_skia_gold",
    targets = [
        "views_examples_unittests",
    ],
    per_test_modifications = {
        "views_examples_unittests": [
            "skia_gold_test",
        ],
    },
)

targets.bundle(
    name = "oreo_isolated_scripts",
    targets = [
        "android_isolated_scripts",
        "chromium_junit_tests_scripts",
        "components_perftests_isolated_scripts",
        "monochrome_public_apk_checker_isolated_script",
        "telemetry_android_minidump_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts_android",
    ],
)

targets.bundle(
    name = "pie_isolated_scripts",
    targets = [
        "android_isolated_scripts",
        "chromium_junit_tests_scripts",
        "components_perftests_isolated_scripts",
        "monochrome_public_apk_checker_isolated_script",
        "telemetry_android_minidump_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts_android",
    ],
)

targets.bundle(
    name = "perfetto_gtests",
    targets = [
        "base_unittests",
        "browser_tests",
        "content_browsertests",
        "perfetto_unittests",
        "services_unittests",
    ],
    per_test_modifications = {
        "browser_tests": targets.mixin(
            args = [
                "--gtest_filter=ChromeTracingDelegateBrowserTest.*",
            ],
        ),
        "content_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 8,
            ),
            android_swarming = targets.swarming(
                shards = 15,
            ),
        ),
    },
)

targets.bundle(
    name = "perfetto_gtests_android",
    targets = [
        "android_browsertests",
        "base_unittests",
        "content_browsertests",
        "perfetto_unittests",
        "services_unittests",
    ],
    per_test_modifications = {
        "android_browsertests": targets.mixin(
            args = [
                "--gtest_filter=StartupMetricsTest.*",
            ],
        ),
        "content_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 8,
            ),
            android_swarming = targets.swarming(
                shards = 15,
            ),
        ),
    },
)

targets.bundle(
    name = "performance_smoke_test_isolated_scripts",
    targets = [
        "performance_test_suite",
    ],
    per_test_modifications = {
        "performance_test_suite": targets.mixin(
            args = [
                "--pageset-repeat=1",
                "--test-shard-map-filename=smoke_test_benchmark_shard_map.json",
            ],
            swarming = targets.swarming(
                shards = 2,
                hard_timeout_sec = 960,
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.bundle(
    name = "pixel_browser_tests_gtests",
    targets = [
        "pixel_browser_tests",
        "pixel_interactive_ui_tests",
    ],
    per_test_modifications = {
        "pixel_browser_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
    },
)

targets.bundle(
    name = "pixel_experimental_browser_tests_gtests",
    targets = [
        "pixel_experimental_browser_tests",
    ],
    per_test_modifications = {
        "pixel_experimental_browser_tests": targets.mixin(
            experiment_percentage = 100,
        ),
    },
)

# TODO(dpranke): These are run on the p/chromium waterfall; they should
# probably be run on other builders, and we should get rid of the p/chromium
# waterfall.
targets.bundle(
    name = "public_build_scripts",
    targets = [
        "checkbins",
    ],
)

targets.bundle(
    name = "pytype_tests",
    targets = [
        "blink_pytype",
        "fuchsia_pytype",
        "gold_common_pytype",
        "gpu_pytype",
        "testing_pytype",
    ],
)

# Rust tests run on all targets.
targets.bundle(
    name = "rust_common_gtests",
    targets = [
        "base_unittests",
        # TODO(https://crbug.com/356914314): Remove `blink_platform_unittests`
        # and `gfx_unittests` if/when Rust PNG is covered by the main
        # waterfall/CQ bots.
        "blink_platform_unittests",
        "gfx_unittests",
        "mojo_rust_integration_unittests",
        "mojo_rust_unittests",
        "rust_gtest_interop_unittests",
        "test_cpp_including_rust_unittests",
        "test_serde_json_lenient",
    ],
    per_test_modifications = {
        "blink_platform_unittests": targets.mixin(
            args = [
                "--test-launcher-bot-mode",
                "--gtest_filter=*PNG*",
            ],
        ),
    },
)

# Rust tests run on non-cross builds.
targets.bundle(
    name = "rust_host_gtests",
    targets = [
        "rust_common_gtests",
    ],
)

targets.bundle(
    name = "rust_native_tests",
    targets = [
        "build_rust_tests",
    ],
)

targets.bundle(
    name = "site_isolation_android_fyi_gtests",
    targets = [
        "site_per_process_android_browsertests",
        "site_per_process_chrome_public_test_apk",
        "site_per_process_chrome_public_unit_test_apk",
        "site_per_process_components_browsertests",
        "site_per_process_components_unittests",
        "site_per_process_content_browsertests",
        "site_per_process_content_shell_test_apk",
        "site_per_process_content_unittests",
        "site_per_process_unit_tests",
    ],
    per_test_modifications = {
        "site_per_process_android_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
        "site_per_process_chrome_public_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "site_per_process_components_unittests": targets.mixin(
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
        "site_per_process_content_browsertests": targets.mixin(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "site_per_process_content_shell_test_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "site_per_process_unit_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
    },
)

targets.bundle(
    name = "swangle_gtests",
    targets = [
        "angle_deqp_egl_tests",
        "angle_deqp_gles2_tests",
        "angle_deqp_gles31_rotate180_tests",
        "angle_deqp_gles31_rotate270_tests",
        "angle_deqp_gles31_rotate90_tests",
        "angle_deqp_gles31_tests",
        "angle_deqp_gles3_rotate180_tests",
        "angle_deqp_gles3_rotate270_tests",
        "angle_deqp_gles3_rotate90_tests",
        "angle_deqp_gles3_tests",
        "angle_deqp_khr_gles2_tests",
        "angle_deqp_khr_gles31_tests",
        "angle_deqp_khr_gles3_tests",
        "angle_end2end_tests",
    ],
    per_test_modifications = {
        "angle_deqp_egl_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles2_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles31_rotate180_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles31_rotate270_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles31_rotate90_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles31_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            swarming = targets.swarming(
                shards = 10,
            ),
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles3_rotate180_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles3_rotate270_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles3_rotate90_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles3_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            swarming = targets.swarming(
                shards = 4,
            ),
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_khr_gles2_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_khr_gles31_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_khr_gles3_tests": targets.mixin(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_end2end_tests": targets.mixin(
            args = [
                "--gtest_filter=*Vulkan_SwiftShader*",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
            use_isolated_scripts_api = True,
        ),
    },
)

targets.bundle(
    name = "system_webview_shell_instrumentation_tests",
    targets = [
        "system_webview_shell_layout_test_apk",
    ],
)

targets.bundle(
    name = "system_webview_wpt_suite",
    targets = "system_webview_wpt",
    per_test_modifications = {
        "system_webview_wpt": targets.mixin(
            args = [
                "--no-wpt-internal",
            ],
            swarming = targets.swarming(
                shards = 25,
                expiration_sec = 18000,
                hard_timeout_sec = 14400,
            ),
        ),
    },
)

targets.bundle(
    name = "telemetry_android_minidump_unittests_isolated_scripts",
    targets = [
        "telemetry_chromium_minidump_unittests",
        "telemetry_monochrome_minidump_unittests",
    ],
)

targets.bundle(
    name = "telemetry_desktop_minidump_unittests_isolated_scripts",
    targets = [
        # Takes ~2.5 minutes of bot time to run.
        "telemetry_desktop_minidump_unittests",
    ],
    per_test_modifications = {
        "telemetry_desktop_minidump_unittests": targets.mixin(
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.bundle(
    name = "telemetry_perf_unittests_isolated_scripts_android",
    targets = [
        "telemetry_perf_unittests_android_chrome",
    ],
    per_test_modifications = {
        "telemetry_perf_unittests_android_chrome": targets.mixin(
            args = [
                # TODO(crbug.com/40129085): Remove this once Crashpad is the default.
                "--extra-browser-args=--enable-crashpad",
            ],
            swarming = targets.swarming(
                shards = 12,
                # https://crbug.com/549140
                idempotent = False,
            ),
        ),
    },
)

targets.bundle(
    name = "telemetry_perf_unittests_isolated_scripts_xvfb",
    targets = [
        "telemetry_perf_unittests",
    ],
    per_test_modifications = {
        "telemetry_perf_unittests": targets.mixin(
            args = [
                # TODO(crbug.com/40129085): Remove this once Crashpad is the default.
                "--extra-browser-args=--enable-crashpad",
                "--xvfb",
            ],
            swarming = targets.swarming(
                shards = 12,
                # https://crbug.com/549140
                idempotent = False,
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.bundle(
    name = "test_traffic_annotation_auditor_script",
    targets = [
        "test_traffic_annotation_auditor",
    ],
)

targets.bundle(
    name = "updater_gtests_linux",
    targets = [
        "updater_tests",
        # 'updater_tests_system' is not yet supported on Linux.
    ],
    per_test_modifications = {
        "updater_tests": [
            "updater-default-pool",
        ],
    },
)

targets.bundle(
    name = "updater_gtests_mac",
    targets = [
        "updater_tests",
        "updater_tests_system",
    ],
    per_test_modifications = {
        "updater_tests": [
            "updater-default-pool",
        ],
        "updater_tests_system": [
            "updater-mac-pool",
        ],
    },
)

targets.bundle(
    name = "updater_gtests_win",
    targets = [
        "updater_tests",
        "updater_tests_system",
    ],
    per_test_modifications = {
        "updater_tests": [
            "integrity_high",
            "updater-default-pool",
        ],
        "updater_tests_system": [
            targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                    hard_timeout_sec = 7200,
                ),
            ),
            "integrity_high",
            "updater-default-pool",
        ],
    },
)

targets.bundle(
    name = "updater_gtests_win_uac",
    targets = [
        "updater_tests_system",
        "updater_tests_win_uac",
    ],
    per_test_modifications = {
        "updater_tests_system": [
            "integrity_high",
            "updater-win-uac-pool",
        ],
        "updater_tests_win_uac": [
            "updater-win-uac-pool",
        ],
    },
)

targets.bundle(
    name = "upload_perfetto",
    targets = [
        "upload_trace_processor",
    ],
)

# Not applicable for android x86 & x64 since the targets here assert
# "enable_vr" in GN which is only true for android arm & arm64.
# For details, see the following files:
#  * //chrome/android/BUILD.gn
#  * //chrome/browser/android/vr/BUILD.gn
#  * //device/vr/buildflags/buildflags.gni
targets.bundle(
    name = "vr_android_specific_chromium_tests",
    targets = [
        "chrome_public_test_vr_apk",
        "vr_android_unittests",
    ],
    per_test_modifications = {
        "chrome_public_test_vr_apk": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "vr_platform_specific_chromium_gtests",
    targets = [
        # Only run on platforms that intend to support WebVR in the near
        # future.
        "vr_common_unittests",
    ],
)

targets.bundle(
    name = "webrtc_chromium_gtests",
    targets = [
        "browser_tests",
        # TODO(crbug.com/246519185) - Py3 incompatible, decide if to keep test.:
        # Uncomment per-test modification if enabling
        # "browser_tests_apprtc",
        "browser_tests_functional",
        "content_browsertests",
        "content_browsertests_sequential",
        "content_browsertests_stress",
        "content_unittests",
        "remoting_unittests",
    ],
    per_test_modifications = {
        "browser_tests": targets.mixin(
            args = [
                "--gtest_filter=WebRtcStatsPerfBrowserTest.*:WebRtcVideoDisplayPerfBrowserTests*:WebRtcVideoQualityBrowserTests*:WebRtcVideoHighBitrateBrowserTest*:WebRtcWebcamBrowserTests*",
                "--ui-test-action-max-timeout=300000",
                "--test-launcher-timeout=350000",
                "--test-launcher-jobs=1",
                "--test-launcher-bot-mode",
                "--test-launcher-print-test-stdio=always",
            ],
        ),
        # "browser_tests_apprtc": targets.mixin(
        #     args = [
        #         "--gtest_filter=WebRtcApprtcBrowserTest.*",
        #         "--test-launcher-jobs=1",
        #     ],
        # ),
        # Run all normal WebRTC content_browsertests. This is mostly so
        # the FYI bots can detect breakages.
        "content_browsertests": targets.mixin(
            args = [
                "--gtest_filter=WebRtc*",
            ],
        ),
        "content_unittests": targets.mixin(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/webrtc.content_unittests.filter",
            ],
        ),
        "remoting_unittests": targets.mixin(
            args = [
                "--gtest_filter=Webrtc*",
            ],
        ),
    },
)

targets.bundle(
    name = "webrtc_chromium_simple_gtests",
    targets = [
        "content_browsertests",
        "content_browsertests_sequential",
    ],
    per_test_modifications = {
        "content_browsertests": targets.mixin(
            args = [
                "--gtest_filter=WebRtc*",
            ],
        ),
    },
)

targets.bundle(
    name = "webview_64_cts_tests_suite",
    targets = [
        "webview_64_cts_tests",
    ],
    per_test_modifications = {
        "webview_64_cts_tests": targets.mixin(
            args = [
                "--store-tombstones",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_bot_all_gtests",
    targets = [
        "system_webview_shell_instrumentation_tests",
        "webview_bot_instrumentation_test_apk_mutations_gtest",
        "webview_bot_instrumentation_test_apk_no_field_trial_gtest",
        "webview_bot_unittests_gtest",
        "webview_cts_tests_gtest",
        "webview_cts_tests_gtest_no_field_trial",
        "webview_ui_instrumentation_tests",
        "webview_ui_instrumentation_tests_no_field_trial",
    ],
)

targets.bundle(
    name = "webview_bot_instrumentation_test_apk_gtest",
    targets = [
        "webview_instrumentation_test_apk",
    ],
    per_test_modifications = {
        "webview_instrumentation_test_apk": targets.mixin(
            args = [
                "--use-apk-under-test-flags-file",
            ],
            swarming = targets.swarming(
                shards = 12,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_bot_instrumentation_test_apk_mutations_gtest",
    targets = [
        "webview_instrumentation_test_apk_mutations",
    ],
    per_test_modifications = {
        "webview_instrumentation_test_apk_mutations": targets.mixin(
            swarming = targets.swarming(
                shards = 12,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_bot_instrumentation_test_apk_no_field_trial_gtest",
    targets = [
        "webview_instrumentation_test_apk_no_field_trial",
    ],
    per_test_modifications = {
        "webview_instrumentation_test_apk_no_field_trial": targets.mixin(
            # TODO(crbug.com/40282232): Make the target infer the correct flag file
            # from the build config.
            args = [
                "--use-apk-under-test-flags-file",
            ],
            swarming = targets.swarming(
                shards = 12,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_bot_unittests_gtest",
    targets = [
        "android_webview_unittests",
    ],
)

targets.bundle(
    name = "webview_cts_tests_gtest",
    targets = [
        "webview_cts_tests",
    ],
    per_test_modifications = {
        "webview_cts_tests": targets.mixin(
            args = [
                "--store-tombstones",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_cts_tests_gtest_no_field_trial",
    targets = [
        "webview_cts_tests_no_field_trial",
    ],
    per_test_modifications = {
        "webview_cts_tests_no_field_trial": targets.mixin(
            args = [
                "--store-tombstones",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_fyi_bot_all_gtests",
    targets = [
        "system_webview_shell_instrumentation_tests",
        "webview_bot_instrumentation_test_apk_mutations_gtest",
        "webview_bot_instrumentation_test_apk_no_field_trial_gtest",
        "webview_bot_unittests_gtest",
        "webview_cts_tests_gtest",
        "webview_cts_tests_gtest_no_field_trial",
        "webview_ui_instrumentation_tests",
        "webview_ui_instrumentation_tests_no_field_trial",
    ],
)

# This target is only to run on Android versions <= Android Q (10).
targets.bundle(
    name = "webview_instrumentation_test_apk_single_process_mode_gtests",
    targets = [
        "webview_instrumentation_test_apk_single_process_mode",
    ],
    per_test_modifications = {
        "webview_instrumentation_test_apk_single_process_mode": targets.mixin(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_native_coverage_bot_gtests",
    targets = [
        "webview_bot_instrumentation_test_apk_mutations_gtest",
        "webview_bot_instrumentation_test_apk_no_field_trial_gtest",
        "webview_bot_unittests_gtest",
    ],
)

targets.bundle(
    name = "webview_trichrome_10_cts_tests_gtest",
    targets = [
        "webview_trichrome_cts_tests_suite",
    ],
    variants = [
        "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
        "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
    ],
)

targets.bundle(
    name = "webview_trichrome_64_32_cts_tests_suite",
    targets = "webview_trichrome_64_32_cts_tests",
    per_test_modifications = {
        "webview_trichrome_64_32_cts_tests": targets.mixin(
            args = [
                "--store-tombstones",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_trichrome_64_cts_field_trial_tests",
    targets = [
        "webview_trichrome_64_cts_tests",
    ],
    per_test_modifications = {
        "webview_trichrome_64_cts_tests": targets.mixin(
            args = [
                "--store-data-dependencies-in-temp",
                "--store-tombstones",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_trichrome_64_cts_gtests",
    targets = [
        "webview_trichrome_64_cts_tests_suite",
        "webview_trichrome_64_cts_tests_no_field_trial_suite",
    ],
)

targets.bundle(
    name = "webview_trichrome_64_cts_hostside_gtests",
    targets = [
        "webview_trichrome_64_cts_hostside_tests",
    ],
    variants = [
        "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
        "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
    ],
)

targets.bundle(
    name = "webview_trichrome_64_cts_tests_no_field_trial_suite",
    targets = [
        "webview_trichrome_64_cts_tests_no_field_trial",
    ],
    per_test_modifications = {
        "webview_trichrome_64_cts_tests_no_field_trial": targets.mixin(
            args = [
                "--store-tombstones",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_trichrome_64_cts_tests_suite",
    targets = "webview_trichrome_64_cts_tests",
    per_test_modifications = {
        "webview_trichrome_64_cts_tests": targets.mixin(
            args = [
                "--store-tombstones",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.bundle(
    name = "webview_trichrome_cts_tests_suite",
    targets = "webview_trichrome_cts_tests",
    per_test_modifications = {
        "webview_trichrome_cts_tests": targets.mixin(
            args = [
                "--store-tombstones",
            ],
        ),
    },
)

targets.bundle(
    name = "webview_ui_instrumentation_tests",
    targets = [
        "webview_ui_test_app_test_apk",
    ],
)

targets.bundle(
    name = "webview_ui_instrumentation_tests_no_field_trial",
    targets = [
        "webview_ui_test_app_test_apk_no_field_trial",
    ],
)

targets.bundle(
    name = "win_optional_gpu_tests_rel_gpu_telemetry_tests",
    targets = [
        targets.bundle(
            targets = "gpu_common_and_optional_telemetry_tests",
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_passthrough_graphite_telemetry_tests",
            variants = [
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webcodecs_telemetry_test",
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
    ],
)

targets.bundle(
    name = "win_optional_gpu_tests_rel_gtests",
    targets = [
        targets.bundle(
            targets = "gpu_default_and_optional_win_media_foundation_specific_gtests",
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_default_and_optional_win_specific_gtests",
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_fyi_and_optional_non_linux_gtests",
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        targets.bundle(
            targets = "gpu_fyi_and_optional_win_specific_gtests",
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
    ],
)

targets.bundle(
    name = "win_optional_gpu_tests_rel_isolated_scripts",
    targets = [
        targets.bundle(
            targets = "gpu_command_buffer_perf_passthrough_isolated_scripts",
            variants = [
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
    ],
)

targets.bundle(
    name = "win_specific_chromium_gtests",
    targets = [
        "chrome_elf_unittests",
        "delayloads_unittests",
        "elevated_tracing_service_unittests",
        "elevation_service_unittests",
        "gcp_unittests",
        "install_static_unittests",
        "installer_util_unittests",
        "notification_helper_unittests",
        "sbox_integration_tests",
        "sbox_unittests",
        "sbox_validation_tests",
        "setup_unittests",
        "updater_tests",
        "updater_tests_system",
        "zucchini_unittests",
    ],
    per_test_modifications = {
        "installer_util_unittests": targets.mixin(
            swarming = targets.swarming(
                dimensions = {
                    "integrity": "high",
                },
            ),
        ),
        "sbox_integration_tests": targets.mixin(
            swarming = targets.swarming(
                dimensions = {
                    "integrity": "high",
                },
            ),
        ),
        "setup_unittests": targets.mixin(
            swarming = targets.swarming(
                dimensions = {
                    "integrity": "high",
                },
            ),
        ),
        "updater_tests_system": targets.mixin(
            swarming = targets.swarming(
                shards = 2,
                hard_timeout_sec = 7200,
            ),
        ),
    },
)

targets.bundle(
    name = "win_specific_xr_perf_tests",
    targets = [
        "xr.webxr.static",
    ],
    per_test_modifications = {
        "xr.webxr.static": targets.mixin(
            experiment_percentage = 100,
        ),
    },
)

targets.bundle(
    name = "wpt_tests_ios_suite",
    targets = [
        "wpt_tests_ios",
    ],
    per_test_modifications = {
        "wpt_tests_ios": targets.mixin(
            args = [
                "--no-wpt-internal",
            ],
            swarming = targets.swarming(
                shards = 36,
                expiration_sec = 18000,
                hard_timeout_sec = 14400,
            ),
        ),
    },
)

targets.bundle(
    name = "wpt_web_tests_android",
    targets = [
        "android_blink_wpt_tests",
    ],
    per_test_modifications = {
        "android_blink_wpt_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
    },
)

targets.bundle(
    name = "wpt_web_tests_webview",
    targets = [
        "webview_blink_wpt_tests",
    ],
    per_test_modifications = {
        "webview_blink_wpt_tests": targets.mixin(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
    },
)
