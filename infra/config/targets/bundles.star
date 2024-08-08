# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains bundle definitions, which are groupings of targets that can
# be referenced by other bundles or by builders. Bundles cannot be used in
# //testing/buildbot

load("//lib/targets.star", "targets")

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

targets.bundle(
    name = "chromium_linux_dbg_isolated_scripts",
    targets = [
        "desktop_chromium_isolated_scripts",
        "linux_specific_chromium_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
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
    name = "fuchsia_arm64_tests",
    targets = [
        "fuchsia_sizes_tests",
        targets.bundle(
            targets = [
                "gpu_validating_telemetry_tests",
                "fuchsia_gtests",
                targets.bundle(
                    targets = "gpu_angle_fuchsia_unittests_isolated_scripts",
                    # Make sure any gtests included get expanded as isolated scripts
                    mixins = targets.mixin(
                        expand_as_isolated_script = True,
                    ),
                ),
            ],
            mixins = [
                "upload_inv_extended_properties",
            ],
        ),
    ],
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
        "courgette_unittests",
        "crypto_unittests",
        "filesystem_service_unittests",
        "web_engine_integration_tests",
        "web_engine_unittests",
    ],
    mixins = [
        "upload_inv_extended_properties",
    ],
)

targets.bundle(
    name = "fuchsia_standard_tests",
    targets = [
        "gpu_validating_telemetry_tests",
        "fuchsia_gtests",
        targets.bundle(
            targets = "fuchsia_isolated_scripts",
            # Make sure any gtests included in fuchsia_isolated_scripts get
            # expanded as isolated scripts
            mixins = targets.mixin(
                expand_as_isolated_script = True,
            ),
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
    name = "webview_bot_all_gtests",
    targets = [
        "system_webview_shell_instrumentation_tests",
        "webview_bot_instrumentation_test_apk_gtest",
        "webview_bot_instrumentation_test_apk_no_field_trial_gtest",
        "webview_bot_unittests_gtest",
        "webview_cts_tests_gtest",
        "webview_cts_tests_gtest_no_field_trial",
        "webview_ui_instrumentation_tests",
        "webview_ui_instrumentation_tests_no_field_trial",
    ],
)
