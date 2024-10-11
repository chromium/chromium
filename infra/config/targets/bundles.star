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
        # No standard tests due to capacity, no Vega tests since it's currently
        # O only.
    ],
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
    name = "android_wpr_record_replay_tests",
    targets = [
        "chrome_java_test_wpr_tests",
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
    name = "chromium_android_scripts",
    targets = [
        "check_network_annotations",
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
            ],
        ),
        "base_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "build_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "chrome_java_test_pagecontroller_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "chrome_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "components_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "content_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "device_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "junit_unit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "keyboard_accessory_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "media_base_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "module_installer_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "net_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "paint_preview_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "password_check_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "password_manager_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "services_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "touch_to_fill_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "ui_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "webapk_client_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "webapk_shell_apk_h2o_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
        "webapk_shell_apk_junit_tests": targets.per_test_modification(
            remove_mixins = [
                "chromium_pixel_2_pie",
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
            ],
        ),
    },
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
    name = "gpu_desktop_passthrough_gtests",
    targets = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_desktop_specific_gtests",
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
                ci_only = True,
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
    name = "gpu_win_gtests",
    targets = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_default_and_optional_win_specific_gtests",
        "gpu_desktop_specific_gtests",
    ],
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

# Please also change ios_code_coverage_tests for any change in this suite.
targets.bundle(
    name = "ios_simulator_full_configs_tests",
    targets = [
        targets.bundle(
            targets = "ios_common_tests",
            variants = [
                "SIM_IPHONE_14_PLUS_17_5",
                "SIM_IPHONE_14_PLUS_18_0",
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
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
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
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
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
                "SIM_IPHONE_SE_3RD_GEN_18_0",
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
                "SIM_IPAD_AIR_6TH_GEN_18_0",
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
                "SIM_IPAD_PRO_7TH_GEN_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_16_4",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPAD_PRO_6TH_GEN_16_4",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
                "SIM_IPHONE_14_PLUS_16_4",
                "SIM_IPHONE_14_PLUS_17_5",
                "SIM_IPHONE_14_PLUS_18_0",
                "SIM_IPHONE_SE_3RD_GEN_16_4",
                "SIM_IPHONE_SE_3RD_GEN_17_5",
                "SIM_IPHONE_SE_3RD_GEN_18_0",
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
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_eg2_cq_tests",
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        targets.bundle(
            targets = "ios_screen_size_dependent_tests",
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
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
    name = "webview_ui_instrumentation_tests_no_field_trial",
    targets = [
        "webview_ui_test_app_test_apk_no_field_trial",
    ],
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
