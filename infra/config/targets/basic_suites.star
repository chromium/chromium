# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains suite definitions that can be used in
# //testing/buildbot/waterfalls.pyl and will also be usable for builders that
# set their tests in starlark (once that is ready). The legacy_ prefix on the
# declarations indicates the capability to be used in //testing/buildbot. Once a
# suite is no longer needed in //testing/buildbot, targets.bundle (which does
# not yet exist) can be used for grouping tests in a more flexible manner.

load("//lib/targets.star", "targets")

targets.legacy_basic_suite(
    name = "android_12_fieldtrial_webview_tests",
    tests = {
        "webview_trichrome_64_cts_tests_no_field_trial": targets.legacy_test_config(
            test = "webview_trichrome_64_cts_tests",
            mixins = [
                "webview_cts_archive",
            ],
            args = [
                "--disable-field-trial-config",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "webview_ui_test_app_test_apk_no_field_trial": targets.legacy_test_config(
            test = "webview_ui_test_app_test_apk",
            args = [
                "--disable-field-trial-config",
            ],
            ci_only = True,
        ),
    },
)

targets.legacy_basic_suite(
    name = "android_ar_gtests",
    tests = {
        "monochrome_public_test_ar_apk": None,
        # Name is vr_*, but actually has AR tests.
        "vr_android_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "android_ddready_vr_gtests",
    tests = {
        "chrome_public_test_vr_apk-ddready-cardboard": targets.legacy_test_config(
            test = "chrome_public_test_vr_apk",
            mixins = [
                "vr_instrumentation_test",
            ],
            args = [
                "--shared-prefs-file=//chrome/android/shared_preference_files/test/vr_cardboard_skipdon_setupcomplete.json",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "chrome_public_test_vr_apk-ddready-ddview": targets.legacy_test_config(
            test = "chrome_public_test_vr_apk",
            mixins = [
                "skia_gold_test",
                "vr_instrumentation_test",
            ],
            args = [
                "--shared-prefs-file=//chrome/android/shared_preference_files/test/vr_ddview_skipdon_setupcomplete.json",
                "--additional-apk=//third_party/gvr-android-sdk/test-apks/vr_keyboard/vr_keyboard_current.apk",
            ],
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
        "chrome_public_test_vr_apk-ddready-don-enabled": targets.legacy_test_config(
            test = "chrome_public_test_vr_apk",
            mixins = [
                "vr_instrumentation_test",
            ],
            args = [
                "--shared-prefs-file=//chrome/android/shared_preference_files/test/vr_ddview_don_setupcomplete.json",
                "--additional-apk=//third_party/gvr-android-sdk/test-apks/vr_keyboard/vr_keyboard_current.apk",
                "--annotation=Restriction=VR_DON_Enabled",
                "--vr-don-enabled",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "android_emulator_specific_chrome_public_tests",
    tests = {
        "chrome_public_test_apk": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
                "emulator-8-cores",  # Use 8-core to shorten test runtime.
            ],
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "chrome_public_unit_test_apk": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "android_isolated_scripts",
    tests = {
        "content_shell_crash_test": targets.legacy_test_config(
            args = [
                "--platform=android",
            ],
            resultdb = targets.resultdb(
                enable = True,
                result_format = "single",
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "android_monochrome_smoke_tests",
    tests = {
        "monochrome_public_bundle_smoke_test": None,
        "monochrome_public_smoke_test": None,
    },
)

targets.legacy_basic_suite(
    name = "android_oreo_standard_gtests",
    tests = {
        "chrome_public_test_apk": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
        "chrome_public_unit_test_apk": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
        ),
        "webview_instrumentation_test_apk": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 5,
                expiration_sec = 10800,
            ),
        ),
    },
)

# TODO(crbug.com/1111436): Deprecate this group in favor of
# android_pie_rel_gtests if/when android Pie capacity is fully restored.
targets.legacy_basic_suite(
    name = "android_pie_rel_reduced_capacity_gtests",
    tests = {
        "android_browsertests": None,
        "blink_platform_unittests": None,
        "cc_unittests": None,
        "content_browsertests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "viz_unittests": None,
        "webview_instrumentation_test_apk": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 7,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "android_smoke_tests",
    tests = {
        "chrome_public_smoke_test": None,
    },
)

targets.legacy_basic_suite(
    name = "android_specific_chromium_gtests",
    tests = {
        "android_browsertests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "android_sync_integration_tests": targets.legacy_test_config(
            args = [
                "--test-launcher-batch-limit=1",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
        "android_webview_unittests": None,
        "content_shell_test_apk": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        # TODO(kbr): these are actually run on many of the GPU bots, which have
        # physical hardware for several of the desktop OSs. Once the GPU JSON
        # generation script is merged with this one, this should be promoted from
        # the Android-specific section.
        "gl_tests_validating": targets.legacy_test_config(
            test = "gl_tests",
            args = [
                "--use-cmd-decoder=validating",
            ],
        ),
        # TODO(kbr): these are actually run on many of the GPU bots, which have
        # physical hardware for several of the desktop OSs. Once the GPU JSON
        # generation script is merged with this one, this should be promoted from
        # the Android-specific section.
        "gl_unittests": None,
        "mojo_test_apk": None,
        "ui_android_unittests": None,
        "webview_instrumentation_test_apk": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 7,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "android_specific_coverage_java_tests",
    tests = {
        "content_shell_test_apk": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "mojo_test_apk": None,
        "webview_instrumentation_test_apk": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 7,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "android_trichrome_smoke_tests",
    tests = {
        "trichrome_chrome_bundle_smoke_test": None,
    },
)

targets.legacy_basic_suite(
    name = "android_webview_gpu_telemetry_tests",
    tests = {
        "android_webview_pixel_skia_gold_test": targets.legacy_test_config(
            telemetry_test_name = "pixel",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "android_wpr_record_replay_tests",
    tests = {
        "chrome_java_test_wpr_tests": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "ash_pixel_gtests",
    tests = {
        "ash_pixeltests": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
            args = [
                "--enable-pixel-output-in-tests",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "aura_gtests",
    tests = {
        "aura_unittests": None,
        "compositor_unittests": None,
        "wm_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "bfcache_android_specific_gtests",
    tests = {
        "bf_cache_android_browsertests": targets.legacy_test_config(
            test = "android_browsertests",
            args = [
                "--disable-features=BackForwardCache",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "bfcache_generic_gtests",
    tests = {
        "bf_cache_content_browsertests": targets.legacy_test_config(
            test = "content_browsertests",
            args = [
                "--disable-features=BackForwardCache",
            ],
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "bfcache_linux_specific_gtests",
    tests = {
        "bf_cache_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            args = [
                "--disable-features=BackForwardCache",
            ],
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "blink_unittests",
    tests = {
        "blink_unit_tests": targets.legacy_test_config(
            test = "blink_unittests",
        ),
        "blink_unit_tests_v2": targets.legacy_test_config(
            test = "blink_unittests_v2",
        ),
    },
)

targets.legacy_basic_suite(
    name = "blink_web_tests_ppapi_isolated_scripts",
    tests = {
        "ppapi_blink_web_tests": targets.legacy_test_config(
            test = "blink_web_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
                "--test-list=../../third_party/blink/web_tests/TestLists/ppapi",
            ],
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "cast_audio_specific_chromium_gtests",
    tests = {
        "cast_audio_backend_unittests": None,
        "cast_base_unittests": None,
        "cast_cast_core_unittests": None,
        "cast_crash_unittests": None,
        "cast_media_unittests": None,
        "cast_shell_browsertests": targets.legacy_test_config(
            args = [
                "--enable-local-file-accesses",
                "--ozone-platform=headless",
                "--no-sandbox",
                "--test-launcher-jobs=1",
            ],
            swarming = targets.swarming(
                enable = False,  # https://crbug.com/861753
            ),
        ),
        "cast_shell_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "cast_junit_tests",
    tests = {
        "cast_base_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
        ),
        "cast_shell_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "cast_video_specific_chromium_gtests",
    tests = {
        "cast_display_settings_unittests": targets.legacy_test_config(
            experiment_percentage = 100,
        ),
        "cast_graphics_unittests": None,
        "views_unittests": targets.legacy_test_config(
            experiment_percentage = 100,
        ),
    },
)

targets.legacy_basic_suite(
    name = "chrome_android_finch_smoke_tests",
    tests = {
        "variations_android_smoke_tests": targets.legacy_test_config(
            test = "variations_desktop_smoke_tests",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--target-platform=android",
            ],
        ),
        "variations_webview_smoke_tests": targets.legacy_test_config(
            test = "variations_desktop_smoke_tests",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--target-platform=webview",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "chrome_finch_smoke_tests",
    tests = {
        "variations_desktop_smoke_tests": targets.legacy_test_config(
            test = "variations_desktop_smoke_tests",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            chromeos_args = [
                "--target-platform=cros",
            ],
            lacros_args = [
                "--target-platform=lacros",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "chrome_isolated_script_tests",
    tests = {
        "chrome_sizes": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
            ),
        ),
        "variations_smoke_tests": targets.legacy_test_config(
            test = "variations_smoke_tests",
            mixins = [
                "skia_gold_test",
            ],
            resultdb = targets.resultdb(
                enable = True,
                result_format = "single",
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chrome_private_code_test_isolated_scripts",
    tests = {
        "chrome_private_code_test": None,
    },
)

targets.legacy_basic_suite(
    name = "chrome_profile_generator_tests",
    tests = {
        "chrome_public_apk_profile_tests": targets.legacy_test_config(
            test = "chrome_public_apk_baseline_profile_generator",
            ci_only = True,
            experiment_percentage = 100,
        ),
    },
)

targets.legacy_basic_suite(
    name = "chrome_public_tests",
    tests = {
        "chrome_public_test_apk": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
            swarming = targets.swarming(
                shards = 19,
            ),
        ),
        "chrome_public_unit_test_apk": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chrome_public_wpt",
    tests = {
        "chrome_public_wpt": targets.legacy_test_config(
            results_handler = "layout tests",
            args = [
                "--no-wpt-internal",
            ],
            swarming = targets.swarming(
                shards = 36,
                expiration_sec = 18000,
                hard_timeout_sec = 14400,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chrome_sizes",
    tests = {
        "chrome_sizes": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chrome_sizes_android",
    tests = {
        "chrome_sizes": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            remove_mixins = [
                "android_r",
                "bullhead",
                "flame",
                "marshmallow",
                "mdarcy",
                "oreo_fleet",
                "oreo_mr1_fleet",
                "pie_fleet",
                "walleye",
            ],
            args = [
                "--platform=android",
            ],
            swarming = targets.swarming(
                dimensions = {
                    "cpu": "x86-64",
                    "os": "Ubuntu-22.04",
                },
            ),
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromedriver_py_tests_isolated_scripts",
    tests = {
        "chromedriver_py_tests": targets.legacy_test_config(
            args = [
                "--test-type=integration",
            ],
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "chromedriver_py_tests_headless_shell": targets.legacy_test_config(
            args = [
                "--test-type=integration",
            ],
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "chromedriver_replay_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "chromeos_annotation_scripts",
    tests = {
        "check_network_annotations": targets.legacy_test_config(
            script = "check_network_annotations.py",
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromeos_browser_all_tast_tests",
    tests = {
        "chrome_all_tast_tests": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--tast-retries=1",
            ],
            swarming = targets.swarming(
                shards = 10,
                # Tast test doesn't always output. See crbug.com/1306300
                io_timeout_sec = 3600,
                idempotent = False,  # https://crbug.com/923426#c27
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromeos_browser_integration_tests",
    tests = {
        "disk_usage_tast_test": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                # Stripping gives more accurate disk usage data.
                "--strip-chrome",
            ],
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/923426#c27
            ),
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromeos_chrome_all_tast_tests",
    tests = {
        "chrome_all_tast_tests": targets.legacy_test_config(
            # `tast_expr` must be a non-empty string to run the tast tests. But the value of
            # would be overridden by `tast_arrt_expr` defined in chromeos/BUILD.gn, so that we
            # put the stub string here.
            tast_expr = "STUB_STRING_TO_RUN_TAST_TESTS",
            test_level_retries = 2,
            timeout_sec = 21600,
            shards = 10,
        ),
    },
)

# Test suite for running critical Tast tests.
targets.legacy_basic_suite(
    name = "chromeos_chrome_criticalstaging_tast_tests",
    tests = {
        "chrome_criticalstaging_tast_tests": targets.legacy_test_config(
            # `tast_expr` must be a non-empty string to run the tast tests. But the value of
            # would be overridden by `tast_arrt_expr` defined in chromeos/BUILD.gn, so that we
            # put the stub string here.
            tast_expr = "STUB_STRING_TO_RUN_TAST_TESTS",
            test_level_retries = 2,
            ci_only = True,
            timeout_sec = 14400,
            experiment_percentage = 100,
            shards = 3,
        ),
    },
)

# Test suite for running disabled Tast tests to collect data to re-enable
# them. The test suite should not be critical to builders.
targets.legacy_basic_suite(
    name = "chromeos_chrome_disabled_tast_tests",
    tests = {
        "chrome_disabled_tast_tests": targets.legacy_test_config(
            # `tast_expr` must be a non-empty string to run the tast tests. But the value of
            # would be overridden by `tast_arrt_expr` defined in chromeos/BUILD.gn, so that we
            # put the stub string here.
            tast_expr = "STUB_STRING_TO_RUN_TAST_TESTS",
            test_level_retries = 1,
            ci_only = True,
            timeout_sec = 14400,
            experiment_percentage = 100,
            shards = 2,
        ),
    },
)

# GTests to run on Chrome OS devices, but not Chrome OS VMs. Any differences
# between this and chromeos_system_friendly_gtests below should only be due
# to resource constraints (ie: not enough devices).
targets.legacy_basic_suite(
    name = "chromeos_device_only_gtests",
    tests = {
        "base_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.base_unittests.filter",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromeos_integration_tests",
    tests = {
        "chromeos_integration_tests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chromeos_isolated_scripts",
    tests = {
        "telemetry_perf_unittests": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--browser=cros-chrome",
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
                "--xvfb",
                # 3 is arbitrary, but if we're having more than 3 of these tests
                # fail in a single shard, then something is probably wrong, so fail
                # fast.
                "--typ-max-failures=3",
            ],
            swarming = targets.swarming(
                shards = 12,
                idempotent = False,  # https://crbug.com/549140
            ),
        ),
        "telemetry_unittests": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--jobs=1",
                "--browser=cros-chrome",
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
                # 3 is arbitrary, but if we're having more than 3 of these tests
                # fail in a single shard, then something is probably wrong, so fail
                # fast.
                "--typ-max-failures=3",
            ],
            swarming = targets.swarming(
                shards = 24,
                idempotent = False,  # https://crbug.com/549140
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromeos_js_code_coverage_browser_tests",
    tests = {
        "chromeos_js_code_coverage_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            swarming = targets.swarming(
                shards = 16,
            ),
        ),
    },
)

# Tests that run on Chrome OS systems (ie: VMs, Chromebooks), *not*
# linux-chromeos.
# NOTE: We only want a small subset of test suites here, because most
# suites assume that they stub out the underlying device hardware.
# https://crbug.com/865693
targets.legacy_basic_suite(
    name = "chromeos_system_friendly_gtests",
    tests = {
        "aura_unittests": targets.legacy_test_config(
            args = [
                "--ozone-platform=headless",
            ],
        ),
        "base_unittests": None,
        "capture_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-jobs=1",
                # Don't run CaptureMJpeg tests on ChromeOS VM because vivid,
                # which is the virtual video capture device, doesn't support MJPEG.
                "--gtest_filter=-*UsingRealWebcam_CaptureMjpeg*",
            ],
        ),
        "cc_unittests": None,
        "crypto_unittests": None,
        "display_unittests": None,
        "fake_libva_driver_unittest": targets.legacy_test_config(
            args = [
                "--env-var",
                "LIBVA_DRIVERS_PATH",
                "./",
                "--env-var",
                "LIBVA_DRIVER_NAME",
                "libfake",
            ],
            experiment_percentage = 100,
        ),
        "google_apis_unittests": None,
        "ipc_tests": None,
        "latency_unittests": None,
        "libcups_unittests": None,
        "media_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.media_unittests.filter",
            ],
        ),
        "midi_unittests": None,
        "mojo_unittests": None,
        # net_unittests has a test-time dependency on vpython. So add a CIPD'ed
        # vpython of the right arch to the task, and tell the test runner to copy
        # it over to the VM before the test runs.
        "net_unittests": targets.legacy_test_config(
            args = [
                "--vpython-dir=../../vpython_dir_linux_amd64",
                # PythonUtils.PythonRunTime (as opposed to Python3RunTime) requires a
                # copy of Python 2, but it's testing test helpers that are only used
                # outside of net_unittests. This bot runs out of space if trying to
                # ship two vpythons, so we exclude Python 2 and the one test which
                # uses it.
                "--gtest_filter=-PythonUtils.PythonRunTime",
            ],
            swarming = targets.swarming(
                shards = 3,
                cipd_packages = [
                    targets.cipd_package(
                        package = "infra/3pp/tools/cpython3/linux-amd64",
                        location = "vpython_dir_linux_amd64",
                        revision = "version:2@3.8.10.chromium.21",
                    ),
                    targets.cipd_package(
                        package = "infra/tools/luci/vpython/linux-amd64",
                        location = "vpython_dir_linux_amd64",
                        revision = "git_revision:0f694cdc06ba054b9960aa1ae9766e45b53d02c1",
                    ),
                ],
            ),
        ),
        "ozone_gl_unittests": targets.legacy_test_config(
            args = [
                "--stop-ui",
            ],
        ),
        "ozone_unittests": None,
        "pdf_unittests": None,
        "printing_unittests": None,
        "profile_provider_unittest": targets.legacy_test_config(
            args = [
                "--stop-ui",
                "--test-launcher-jobs=1",
            ],
        ),
        "rust_gtest_interop_unittests": None,
        "sql_unittests": None,
        "url_unittests": None,
        "vaapi_unittest": targets.legacy_test_config(
            args = [
                "--stop-ui",
                # Tell libva to do dummy encoding/decoding. For more info, see:
                # https://github.com/intel/libva/blob/master/va/va_fool.c#L47
                "--env-var",
                "LIBVA_DRIVERS_PATH",
                "./",
                "--env-var",
                "LIBVA_DRIVER_NAME",
                "libfake",
                "--gtest_filter=\"VaapiTest.*\"",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_android_scripts",
    tests = {
        "check_network_annotations": targets.legacy_test_config(
            script = "check_network_annotations.py",
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_android_webkit_gtests",
    tests = {
        "blink_heap_unittests": None,
        "webkit_unit_tests": targets.legacy_test_config(
            test = "blink_unittests",
        ),
        "webkit_unit_tests_v2": targets.legacy_test_config(
            test = "blink_unittests_v2",
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_dev_android_gtests",
    tests = {
        "chrome_public_smoke_test": None,
    },
)

targets.legacy_basic_suite(
    name = "chromium_dev_desktop_gtests",
    tests = {
        "base_unittests": None,
        "content_browsertests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
        "content_unittests": None,
        "interactive_ui_tests": None,
        "net_unittests": None,
        "rust_gtest_interop_unittests": None,
        "unit_tests": None,
    },
)

targets.legacy_basic_suite(
    name = "chromium_dev_linux_gtests",
    tests = {
        "base_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "cores": "8",
                },
            ),
        ),
        "browser_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 8,
                dimensions = {
                    "cores": "8",
                },
            ),
        ),
        "content_browsertests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 5,
                dimensions = {
                    "cores": "8",
                },
            ),
        ),
        "content_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "cores": "2",
                },
            ),
        ),
        "interactive_ui_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
                dimensions = {
                    "cores": "8",
                },
            ),
        ),
        "net_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "cores": "8",
                },
            ),
        ),
        "rust_gtest_interop_unittests": None,
        "unit_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "cores": "2",
                },
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_gtests",
    tests = {
        "absl_hardening_tests": None,
        "angle_unittests": targets.legacy_test_config(
            android_args = [
                "-v",
            ],
            use_isolated_scripts_api = True,
        ),
        "base_unittests": None,
        "blink_common_unittests": None,
        "blink_heap_unittests": None,
        "blink_platform_unittests": None,
        "boringssl_crypto_tests": None,
        "boringssl_ssl_tests": None,
        "capture_unittests": targets.legacy_test_config(
            args = [
                "--gtest_filter=-*UsingRealWebcam*",
            ],
        ),
        "cast_unittests": None,
        "components_browsertests": None,
        "components_unittests": targets.legacy_test_config(
            android_swarming = targets.swarming(
                shards = 6,
            ),
        ),
        "content_browsertests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 8,
            ),
            android_swarming = targets.swarming(
                shards = 15,
            ),
        ),
        "content_unittests": targets.legacy_test_config(
            android_swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "crashpad_tests": None,
        "crypto_unittests": None,
        "env_chromium_unittests": targets.legacy_test_config(
            ci_only = True,
        ),
        "events_unittests": None,
        "gcm_unit_tests": None,
        "gin_unittests": None,
        "google_apis_unittests": None,
        "gpu_unittests": None,
        "gwp_asan_unittests": None,
        "ipc_tests": None,
        "latency_unittests": None,
        "leveldb_unittests": targets.legacy_test_config(
            ci_only = True,
        ),
        "libjingle_xmpp_unittests": None,
        "liburlpattern_unittests": None,
        "media_unittests": None,
        "midi_unittests": None,
        "mojo_unittests": None,
        "net_unittests": targets.legacy_test_config(
            android_swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "perfetto_unittests": None,
        # TODO(crbug.com/1459686): Enable this.
        # "rust_gtest_interop_unittests": None,
        "services_unittests": None,
        "shell_dialogs_unittests": None,
        "skia_unittests": None,
        "sql_unittests": None,
        "storage_unittests": None,
        "ui_base_unittests": None,
        "ui_touch_selection_unittests": None,
        "url_unittests": None,
        "webkit_unit_tests": targets.legacy_test_config(
            test = "blink_unittests",
            android_swarming = targets.swarming(
                shards = 6,
            ),
        ),
        "webkit_unit_tests_v2": targets.legacy_test_config(
            test = "blink_unittests_v2",
            android_swarming = targets.swarming(
                shards = 6,
            ),
        ),
        "wtf_unittests": None,
        "zlib_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "chromium_gtests_for_devices_with_graphical_output",
    tests = {
        "cc_unittests": None,
        "device_unittests": None,
        "display_unittests": None,
        "gfx_unittests": None,
        "unit_tests": targets.legacy_test_config(
            android_swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "viz_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "chromium_gtests_for_linux_and_chromeos_only",
    tests = {
        "dbus_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "chromium_gtests_for_linux_and_mac_only",
    tests = {
        "openscreen_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "chromium_gtests_for_linux_only",
    tests = {
        "ozone_unittests": None,
        "ozone_x11_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "chromium_gtests_for_win_and_linux_only",
    tests = {
        # pthreadpool is only built on Windows and Linux platforms, that is
        # determined by `build_tflite_with_xnnpack` defined in
        # third_party/tflite/features.gni.
        "pthreadpool_unittests": targets.legacy_test_config(
            ci_only = True,
        ),
    },
)

# Multiscreen tests for desktop platform (Windows).
targets.legacy_basic_suite(
    name = "chromium_gtests_for_windows_multiscreen",
    tests = {
        "multiscreen_interactive_ui_tests": targets.legacy_test_config(
            test = "interactive_ui_tests",
            args = [
                "--windows-virtual-display-driver",
                "--gtest_filter=*MultiScreen*:*VirtualDisplayWinUtil*",
            ],
            swarming = targets.swarming(
                dimensions = {
                    "pool": "chromium.tests.multiscreen",
                },
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_ios_scripts",
    tests = {
        "check_static_initializers": targets.legacy_test_config(
            script = "check_static_initializers.py",
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_junit_tests_scripts",
    tests = {
        "android_webview_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "base_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "build_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "chrome_java_test_pagecontroller_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "chrome_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "components_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "content_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "device_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "junit_unit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "keyboard_accessory_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "media_base_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "module_installer_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "net_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "paint_preview_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "password_check_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "password_manager_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "services_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "touch_to_fill_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "ui_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "webapk_client_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "webapk_shell_apk_h2o_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
        "webapk_shell_apk_junit_tests": targets.legacy_test_config(
            mixins = [
                "x86-64",
                "linux-jammy",
                "junit-swarming-emulator",
            ],
            remove_mixins = [
                "emulator-4-cores",
                "nougat-x86-emulator",
                "oreo-x86-emulator",
                "walleye",
                "pie_fleet",
            ],
            use_isolated_scripts_api = True,
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_linux_scripts",
    tests = {
        "check_network_annotations": targets.legacy_test_config(
            script = "check_network_annotations.py",
        ),
        "check_static_initializers": targets.legacy_test_config(
            script = "check_static_initializers.py",
        ),
        "checkdeps": targets.legacy_test_config(
            script = "checkdeps.py",
        ),
        "checkperms": targets.legacy_test_config(
            script = "checkperms.py",
        ),
        "headless_python_unittests": targets.legacy_test_config(
            script = "headless_python_unittests.py",
        ),
        "metrics_python_tests": targets.legacy_test_config(
            script = "metrics_python_tests.py",
        ),
        "webkit_lint": targets.legacy_test_config(
            script = "blink_lint_expectations.py",
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_mac_scripts",
    tests = {
        "check_static_initializers": targets.legacy_test_config(
            script = "check_static_initializers.py",
        ),
        "metrics_python_tests": targets.legacy_test_config(
            script = "metrics_python_tests.py",
        ),
        "webkit_lint": targets.legacy_test_config(
            script = "blink_lint_expectations.py",
        ),
    },
)

# On some bots we don't have capacity to run all standard tests (for example
# Android Pie), however there are tracing integration tests we want to
# ensure are still working.
targets.legacy_basic_suite(
    name = "chromium_tracing_gtests",
    tests = {
        "services_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "chromium_web_tests_brfetch_isolated_scripts",
    tests = {
        # brfetch_blink_web_tests provides coverage for
        # running Layout Tests with BackgroundResourceFetch feature.
        "brfetch_blink_web_tests": targets.legacy_test_config(
            test = "blink_web_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                "--flag-specific=background-resource-fetch",
                "--skipped=always",
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 1,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
            experiment_percentage = 100,
        ),
        # brfetch_blink_wpt_tests provides coverage for
        # running Layout Tests with BackgroundResourceFetch feature.
        "brfetch_blink_wpt_tests": targets.legacy_test_config(
            test = "blink_wpt_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                "--flag-specific=background-resource-fetch",
                "--skipped=always",
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 3,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
            experiment_percentage = 100,
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_web_tests_high_dpi_isolated_scripts",
    tests = {
        # high_dpi_blink_web_tests provides coverage for
        # running Layout Tests with forced device scale factor.
        "high_dpi_blink_web_tests": targets.legacy_test_config(
            test = "blink_web_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                "--flag-specific=highdpi",
                "--skipped=always",
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
        # high_dpi_blink_wpt_tests provides coverage for
        # running Layout Tests with forced device scale factor.
        "high_dpi_blink_wpt_tests": targets.legacy_test_config(
            test = "blink_wpt_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                "--flag-specific=highdpi",
                "--skipped=always",
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 3,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_webkit_isolated_scripts",
    tests = {
        "blink_web_tests": targets.legacy_test_config(
            test = "blink_web_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 5,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
        "blink_wpt_tests": targets.legacy_test_config(
            test = "blink_wpt_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 7,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_win_scripts",
    tests = {
        "check_network_annotations": targets.legacy_test_config(
            script = "check_network_annotations.py",
        ),
        "metrics_python_tests": targets.legacy_test_config(
            script = "metrics_python_tests.py",
        ),
        "webkit_lint": targets.legacy_test_config(
            script = "blink_lint_expectations.py",
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_wpt_tests_isolated_scripts",
    tests = {
        "chrome_wpt_tests": targets.legacy_test_config(
            test = "chrome_wpt_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--test-type",
                "testharness",
                "reftest",
                "crashtest",
                "print-reftest",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 15,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "clang_tot_gtests",
    tests = {
        "base_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "client_v8_chromium_gtests",
    tests = {
        "app_shell_unittests": None,
        "browser_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "chrome_app_unittests": None,
        "chromedriver_unittests": None,
        "components_browsertests": None,
        "components_unittests": None,
        "compositor_unittests": None,
        "content_browsertests": None,
        "content_unittests": None,
        "device_unittests": None,
        "extensions_browsertests": None,
        "extensions_unittests": None,
        "gcm_unit_tests": None,
        "gin_unittests": None,
        "google_apis_unittests": None,
        "gpu_unittests": None,
        "headless_browsertests": None,
        "headless_unittests": None,
        "interactive_ui_tests": None,
        "net_unittests": None,
        "pdf_unittests": None,
        "remoting_unittests": None,
        "services_unittests": None,
        "sync_integration_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "unit_tests": None,
    },
)

targets.legacy_basic_suite(
    name = "client_v8_chromium_isolated_scripts",
    tests = {
        "content_shell_crash_test": targets.legacy_test_config(
            resultdb = targets.resultdb(
                enable = True,
                result_format = "single",
            ),
        ),
        "telemetry_gpu_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "telemetry_perf_unittests": targets.legacy_test_config(
            args = [
                "--xvfb",
                # TODO(crbug.com/1077284): Remove this once Crashpad is the default.
                "--extra-browser-args=--enable-crashpad",
            ],
            swarming = targets.swarming(
                shards = 12,
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "telemetry_unittests": targets.legacy_test_config(
            args = [
                "--jobs=1",
                # Disable GPU compositing, telemetry_unittests runs on VMs.
                # https://crbug.com/871955
                "--extra-browser-args=--disable-gpu",
            ],
            swarming = targets.swarming(
                shards = 4,
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "components_perftests_isolated_scripts",
    tests = {
        "components_perftests": targets.legacy_test_config(
            args = [
                "--gtest-benchmark-name=components_perftests",
            ],
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
                args = [
                    "--smoke-test-mode",
                ],
            ),
        ),
    },
)

# TODO(crbug.com/1444855): Delete the cr23_{linux,mac,win}_gtest suites
# after the ChromeRefresh2023 is fully rolled out.
targets.legacy_basic_suite(
    name = "cr23_linux_gtests",
    tests = {
        "cr23_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            mixins = [
                "chrome-refresh-2023",
            ],
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/cr23.linux.cr23_browser_tests.filter",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "cr23_interactive_ui_tests": targets.legacy_test_config(
            test = "interactive_ui_tests",
            mixins = [
                "chrome-refresh-2023",
            ],
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/cr23.linux.cr23_interactive_ui_tests.filter",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "cr23_views_unittests": targets.legacy_test_config(
            test = "views_unittests",
            mixins = [
                "chrome-refresh-2023",
            ],
            ci_only = True,
        ),
    },
)

targets.legacy_basic_suite(
    name = "cr23_mac_gtests",
    tests = {
        "cr23_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            mixins = [
                "chrome-refresh-2023",
            ],
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/cr23.mac.cr23_browser_tests.filter",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "cr23_interactive_ui_tests": targets.legacy_test_config(
            test = "interactive_ui_tests",
            mixins = [
                "chrome-refresh-2023",
            ],
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/cr23.mac.cr23_interactive_ui_tests.filter",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "cr23_views_unittests": targets.legacy_test_config(
            test = "views_unittests",
            mixins = [
                "chrome-refresh-2023",
            ],
            ci_only = True,
        ),
    },
)

targets.legacy_basic_suite(
    name = "cr23_pixel_browser_tests_gtests",
    tests = {
        "cr23_pixel_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            mixins = [
                "skia_gold_test",
                "chrome-refresh-2023",
            ],
            args = [
                "--browser-ui-tests-verify-pixels",
                "--enable-pixel-output-in-tests",
                "--test-launcher-filter-file=../../testing/buildbot/filters/pixel_tests.filter;../../testing/buildbot/filters/cr23.win.cr23_browser_tests.filter",
                "--test-launcher-jobs=1",
            ],
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "cr23_pixel_interactive_ui_tests": targets.legacy_test_config(
            test = "interactive_ui_tests",
            mixins = [
                "skia_gold_test",
                "chrome-refresh-2023",
            ],
            args = [
                "--browser-ui-tests-verify-pixels",
                "--enable-pixel-output-in-tests",
                "--test-launcher-filter-file=../../testing/buildbot/filters/pixel_tests.filter;../../testing/buildbot/filters/cr23.win.cr23_interactive_ui_tests.filter",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "cr23_win_gtests",
    tests = {
        "cr23_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            mixins = [
                "chrome-refresh-2023",
            ],
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/cr23.win.cr23_browser_tests.filter",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "cr23_interactive_ui_tests": targets.legacy_test_config(
            test = "interactive_ui_tests",
            mixins = [
                "chrome-refresh-2023",
            ],
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/cr23.win.cr23_interactive_ui_tests.filter",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "cr23_views_unittests": targets.legacy_test_config(
            test = "views_unittests",
            mixins = [
                "chrome-refresh-2023",
            ],
            ci_only = True,
        ),
    },
)

targets.legacy_basic_suite(
    name = "cronet_gtests",
    tests = {
        "cronet_sample_test_apk": None,
        "cronet_smoketests_missing_native_library_instrumentation_apk": None,
        "cronet_smoketests_platform_only_instrumentation_apk": None,
        "cronet_test_instrumentation_apk": targets.legacy_test_config(
            mixins = [
                "emulator-enable-network",
            ],
        ),
        "cronet_tests_android": None,
        "cronet_unittests_android": None,
        "net_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
    },
)

# TODO(b/314092564): Merge with cronet_gtests test suite
# after this test-suite has run on CI for a week and proved its
# stability.
targets.legacy_basic_suite(
    name = "cronet_gtests_and_proguarded_smoketest",
    tests = {
        "cronet_sample_test_apk": None,
        "cronet_smoketests_apk": None,  # This is the only new addition to this test-suite.
        "cronet_smoketests_missing_native_library_instrumentation_apk": None,
        "cronet_smoketests_platform_only_instrumentation_apk": None,
        "cronet_test_instrumentation_apk": targets.legacy_test_config(
            mixins = [
                "emulator-enable-network",
            ],
        ),
        "cronet_tests_android": None,
        "cronet_unittests_android": None,
        "net_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "cronet_resource_sizes",
    tests = {
        "resource_sizes_cronet_sample_apk": targets.legacy_test_config(
            swarming = targets.swarming(
                # This suite simply analyzes build targets without running them.
                # It can thus run on a standard linux machine w/o a device.
                dimensions = {
                    "os": "Ubuntu-22.04",
                    "cpu": "x86-64",
                },
            ),
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
            ),
            resultdb = targets.resultdb(
                enable = True,
                result_format = "single",
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "cronet_sizes",
    tests = {
        "cronet_sizes": targets.legacy_test_config(
            remove_mixins = [
                "android_r",
                "bullhead",
                "flame",
                "marshmallow",
                "mdarcy",
                "oreo_fleet",
                "oreo_mr1_fleet",
                "pie_fleet",
                "walleye",
            ],
            swarming = targets.swarming(
                # This suite simply analyzes build targets without running them.
                # It can thus run on a standard linux machine w/o a device.
                dimensions = {
                    "os": "Ubuntu-22.04",
                    "cpu": "x86-64",
                },
            ),
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
            ),
            resultdb = targets.resultdb(
                enable = True,
                result_format = "single",
                result_file = "${ISOLATED_OUTDIR}/sizes/test_results.json",
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "desktop_chromium_isolated_scripts",
    tests = {
        "blink_python_tests": targets.legacy_test_config(
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "blink_web_tests": targets.legacy_test_config(
            test = "blink_web_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 5,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
        "blink_wpt_tests": targets.legacy_test_config(
            test = "blink_wpt_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 7,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
        "content_shell_crash_test": targets.legacy_test_config(
            resultdb = targets.resultdb(
                enable = True,
                result_format = "single",
            ),
        ),
        "flatbuffers_unittests": targets.legacy_test_config(
            resultdb = targets.resultdb(
                enable = True,
                result_format = "single",
            ),
        ),
        "grit_python_unittests": targets.legacy_test_config(
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "telemetry_gpu_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "telemetry_unittests": targets.legacy_test_config(
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
        "views_perftests": targets.legacy_test_config(
            args = [
                "--gtest-benchmark-name=views_perftests",
            ],
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
                args = [
                    "--smoke-test-mode",
                ],
            ),
        ),
    },
)

# Script tests that only need to run on one builder per desktop platform.
targets.legacy_basic_suite(
    name = "desktop_once_isolated_scripts",
    tests = {
        "test_env_py_unittests": targets.legacy_test_config(
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "xvfb_py_unittests": targets.legacy_test_config(
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "devtools_browser_tests",
    tests = {
        "devtools_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            args = [
                "--gtest_filter=*DevTools*",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "devtools_webkit_and_tab_target_isolated_scripts",
    tests = {
        "blink_web_tests": targets.legacy_test_config(
            test = "blink_web_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 5,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
        "blink_web_tests_dt_tab_target": targets.legacy_test_config(
            test = "blink_web_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                "--flag-specific=devtools-tab-target",
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
                "http/tests/devtools",
            ],
            swarming = targets.swarming(
                shards = 5,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
        "blink_wpt_tests": targets.legacy_test_config(
            test = "blink_wpt_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 7,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "devtools_webkit_isolated_scripts",
    tests = {
        "blink_web_tests": targets.legacy_test_config(
            test = "blink_web_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 5,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
        "blink_wpt_tests": targets.legacy_test_config(
            test = "blink_wpt_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 7,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "fieldtrial_android_tests",
    tests = {
        "android_browsertests_no_fieldtrial": targets.legacy_test_config(
            test = "android_browsertests",
            args = [
                "--disable-field-trial-config",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "fieldtrial_browser_tests",
    tests = {
        "browser_tests_no_field_trial": targets.legacy_test_config(
            test = "browser_tests",
            args = [
                "--disable-field-trial-config",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "components_browsertests_no_field_trial": targets.legacy_test_config(
            test = "components_browsertests",
            args = [
                "--disable-field-trial-config",
            ],
            ci_only = True,
        ),
        "interactive_ui_tests_no_field_trial": targets.legacy_test_config(
            test = "interactive_ui_tests",
            args = [
                "--disable-field-trial-config",
            ],
            ci_only = True,
        ),
        "sync_integration_tests_no_field_trial": targets.legacy_test_config(
            test = "sync_integration_tests",
            args = [
                "--disable-field-trial-config",
            ],
            ci_only = True,
        ),
    },
)

targets.legacy_basic_suite(
    name = "finch_smoke_tests",
    tests = {
        # TODO(crbug.com/1227222): Change this to the actual finch smoke test
        # once it exists.
        "base_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "fuchsia_accessibility_content_browsertests",
    tests = {
        "accessibility_content_browsertests": targets.legacy_test_config(
            test = "content_browsertests",
            args = [
                "--gtest_filter=*All/DumpAccessibility*/fuchsia*",
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

targets.legacy_basic_suite(
    name = "fuchsia_chrome_small_gtests",
    tests = {
        "courgette_unittests": None,
        "extensions_unittests": None,
        "headless_unittests": None,
        "message_center_unittests": None,
        "views_examples_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.views_examples_unittests.filter",
            ],
        ),
        "views_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.views_unittests.filter",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "fuchsia_common_gtests",
    tests = {
        "absl_hardening_tests": None,
        "accessibility_unittests": None,
        "aura_unittests": None,
        "base_unittests": None,
        "blink_common_unittests": None,
        "blink_fuzzer_unittests": None,
        "blink_heap_unittests": None,
        "blink_platform_unittests": None,
        "blink_unittests": None,
        "blink_unittests_v2": None,
        "boringssl_crypto_tests": None,
        "boringssl_ssl_tests": None,
        "capture_unittests": None,
        "components_browsertests": targets.legacy_test_config(
            args = [
                "--test-arg=--disable-gpu",
                "--test-arg=--headless",
                "--test-arg=--ozone-platform=headless",
            ],
        ),
        "components_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "compositor_unittests": None,
        "content_browsertests": targets.legacy_test_config(
            args = [
                "--gtest_filter=-All/DumpAccessibility*/fuchsia",
                "--test-arg=--disable-gpu",
                "--test-arg=--headless",
                "--test-arg=--ozone-platform=headless",
            ],
            swarming = targets.swarming(
                shards = 14,
            ),
        ),
        "content_unittests": None,
        "crypto_unittests": None,
        "events_unittests": None,
        "filesystem_service_unittests": None,
        "gcm_unit_tests": None,
        "gin_unittests": None,
        "google_apis_unittests": None,
        "gpu_unittests": None,
        "gwp_asan_unittests": None,
        "headless_browsertests": None,
        "ipc_tests": None,
        "latency_unittests": None,
        "media_unittests": None,
        "midi_unittests": None,
        "mojo_unittests": None,
        "native_theme_unittests": None,
        "net_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.net_unittests.filter",
            ],
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
        "ozone_gl_unittests": targets.legacy_test_config(
            args = [
                "--test-arg=--ozone-platform=headless",
            ],
        ),
        "ozone_unittests": None,
        "perfetto_unittests": None,
        # TODO(crbug.com/1459686): Enable this.
        # "rust_gtest_interop_unittests": None,
        "services_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.services_unittests.filter",
            ],
        ),
        "shell_dialogs_unittests": None,
        "skia_unittests": None,
        "snapshot_unittests": None,
        "sql_unittests": None,
        "storage_unittests": None,
        "ui_base_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/fuchsia.ui_base_unittests.filter",
            ],
        ),
        "ui_touch_selection_unittests": None,
        "ui_unittests": None,
        "url_unittests": None,
        "wm_unittests": None,
        "wtf_unittests": None,
        "zlib_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "fuchsia_common_gtests_with_graphical_output",
    tests = {
        "cc_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "display_unittests": None,
        "gfx_unittests": None,
        "viz_unittests": targets.legacy_test_config(
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
targets.legacy_basic_suite(
    name = "fuchsia_facility_gtests",
    tests = {
        "aura_unittests": None,
        "blink_common_unittests": None,
        "courgette_unittests": None,
        "crypto_unittests": None,
        "filesystem_service_unittests": None,
        "web_engine_integration_tests": None,
        "web_engine_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "fuchsia_sizes_tests",
    tests = {
        "fuchsia_sizes": targets.legacy_test_config(
            args = [
                "--sizes-path",
                "tools/fuchsia/size_tests/fyi_sizes_smoketest.json",
            ],
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gl_gtests_passthrough",
    tests = {
        "gl_tests_passthrough": targets.legacy_test_config(
            test = "gl_tests",
            args = [
                "--use-cmd-decoder=passthrough",
            ],
            linux_args = [
                "--no-xvfb",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "gl_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "goma_gtests",
    tests = {
        "base_unittests": None,
        "content_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "goma_mac_gtests",
    tests = {
        "base_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "cpu": "x86-64",
                    "os": "Mac-13",
                },
            ),
        ),
        "content_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "cpu": "x86-64",
                    "os": "Mac-13",
                },
            ),
        ),
    },
)

# BEGIN tests which run on the GPU bots

targets.legacy_basic_suite(
    name = "gpu_angle_fuchsia_unittests_isolated_scripts",
    tests = {
        "angle_unittests": targets.legacy_test_config(
            mixins = [
                "fuchsia_logs",
            ],
            args = [
                "bin/run_angle_unittests",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_angle_ios_end2end_gtests",
    tests = {
        "angle_end2end_tests": targets.legacy_test_config(
            args = [
                "--release",
            ],
            use_isolated_scripts_api = True,
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_angle_ios_white_box_gtests",
    tests = {
        "angle_white_box_tests": targets.legacy_test_config(
            args = [
                "--release",
            ],
            use_isolated_scripts_api = True,
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_angle_unit_gtests",
    tests = {
        "angle_unittests": targets.legacy_test_config(
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

# The command buffer perf tests are only run on Windows.
# They are mostly driver and platform independent.
targets.legacy_basic_suite(
    name = "gpu_command_buffer_perf_passthrough_isolated_scripts",
    tests = {
        "passthrough_command_buffer_perftests": targets.legacy_test_config(
            test = "command_buffer_perftests",
            args = [
                "--gtest-benchmark-name=passthrough_command_buffer_perftests",
                "-v",
                "--use-cmd-decoder=passthrough",
                "--use-angle=gl-null",
                "--fast-run",
            ],
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
                args = [
                    "--smoke-test-mode",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_common_and_optional_telemetry_tests",
    tests = {
        "info_collection_tests": targets.legacy_test_config(
            telemetry_test_name = "info_collection",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "$$MAGIC_SUBSTITUTION_GPUExpectedVendorId",
                "$$MAGIC_SUBSTITUTION_GPUExpectedDeviceId",
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--force_high_performance_gpu",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "trace_test": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
    },
)

# GPU gtests which run on both the main and FYI waterfalls.
targets.legacy_basic_suite(
    name = "gpu_common_gtests_passthrough",
    tests = {
        "gl_tests_passthrough": targets.legacy_test_config(
            test = "gl_tests",
            args = [
                "--use-cmd-decoder=passthrough",
                "--use-gl=angle",
            ],
            chromeos_args = [
                "--stop-ui",
            ],
            desktop_args = [
                "--use-gpu-in-tests",
            ],
            linux_args = [
                "--no-xvfb",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "gl_unittests": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
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
    },
)

targets.legacy_basic_suite(
    name = "gpu_common_gtests_validating",
    tests = {
        "gl_tests_validating": targets.legacy_test_config(
            test = "gl_tests",
            args = [
                "--use-cmd-decoder=validating",
            ],
            chromeos_args = [
                "--stop-ui",
                "$$MAGIC_SUBSTITUTION_ChromeOSGtestFilterFile",
            ],
            desktop_args = [
                "--use-gpu-in-tests",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
        "gl_unittests": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
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
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_common_isolated_scripts",
    tests = {
        # Test that expectations files are well-formed.
        "telemetry_gpu_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

# GPU gtests that test only Dawn
targets.legacy_basic_suite(
    name = "gpu_dawn_gtests",
    tests = {
        "dawn_end2end_implicit_device_sync_tests": targets.legacy_test_config(
            test = "dawn_end2end_tests",
            mixins = [
                "dawn_end2end_gpu_test",
            ],
            args = [
                "--enable-implicit-device-sync",
            ],
            linux_args = [
                "--no-xvfb",
            ],
            ci_only = True,  # https://crbug.com/dawn/1749
        ),
        "dawn_end2end_skip_validation_tests": targets.legacy_test_config(
            test = "dawn_end2end_tests",
            mixins = [
                "dawn_end2end_gpu_test",
            ],
            args = [
                "--enable-toggles=skip_validation",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
        "dawn_end2end_tests": targets.legacy_test_config(
            mixins = [
                "dawn_end2end_gpu_test",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
        "dawn_end2end_wire_tests": targets.legacy_test_config(
            test = "dawn_end2end_tests",
            mixins = [
                "dawn_end2end_gpu_test",
            ],
            args = [
                "--use-wire",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_gtests_no_dxc",
    tests = {
        "dawn_end2end_no_dxc_tests": targets.legacy_test_config(
            test = "dawn_end2end_tests",
            mixins = [
                "dawn_end2end_gpu_test",
            ],
            args = [
                "--disable-toggles=use_dxc",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_gtests_no_dxc_with_validation",
    tests = {
        "dawn_end2end_no_dxc_validation_layers_tests": targets.legacy_test_config(
            test = "dawn_end2end_tests",
            mixins = [
                "dawn_end2end_gpu_test",
            ],
            args = [
                "--disable-toggles=use_dxc",
                "--enable-backend-validation",
            ],
        ),
    },
)

# GPU gtests that test only Dawn with backend validation layers
targets.legacy_basic_suite(
    name = "gpu_dawn_gtests_with_validation",
    tests = {
        "dawn_end2end_validation_layers_tests": targets.legacy_test_config(
            test = "dawn_end2end_tests",
            mixins = [
                "dawn_end2end_gpu_test",
            ],
            args = [
                "--enable-backend-validation",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_perf_smoke_isolated_scripts",
    tests = {
        "dawn_perf_tests": targets.legacy_test_config(
            args = [
                # Tell the tests to only run one step for faster iteration.
                "--override-steps=1",
                "--gtest-benchmark-name=dawn_perf_tests",
                "-v",
            ],
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
                args = [
                    # Does not upload to the perf dashboard
                    "--smoke-test-mode",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_web_platform_webgpu_cts_force_swiftshader",
    tests = {
        "webgpu_swiftshader_web_platform_cts_tests": targets.legacy_test_config(
            telemetry_test_name = "webgpu_cts",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            args = [
                "--use-webgpu-adapter=swiftshader",
                "--test-filter=*web_platform*",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "webgpu_swiftshader_web_platform_cts_with_validation_tests": targets.legacy_test_config(
            telemetry_test_name = "webgpu_cts",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            args = [
                "--use-webgpu-adapter=swiftshader",
                "--test-filter=*web_platform*",
                "--enable-dawn-backend-validation",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_webgpu_blink_web_tests",
    tests = {
        "webgpu_blink_web_tests": targets.legacy_test_config(
            test = "webgpu_blink_web_tests",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_cts",
            ],
            args = [
                "--flag-specific=webgpu",
            ],
        ),
        "webgpu_blink_web_tests_with_backend_validation": targets.legacy_test_config(
            test = "webgpu_blink_web_tests",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_cts",
            ],
            args = [
                "--flag-specific=webgpu-with-backend-validation",
                # Increase the timeout when using backend validation layers (crbug.com/1208253)
                "--timeout-ms=30000",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_webgpu_blink_web_tests_force_swiftshader",
    tests = {
        "webgpu_swiftshader_blink_web_tests": targets.legacy_test_config(
            test = "webgpu_blink_web_tests",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_cts",
            ],
            args = [
                "--flag-specific=webgpu-swiftshader",
            ],
        ),
        "webgpu_swiftshader_blink_web_tests_with_backend_validation": targets.legacy_test_config(
            test = "webgpu_blink_web_tests",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_cts",
            ],
            args = [
                "--flag-specific=webgpu-swiftshader-with-backend-validation",
                # Increase the timeout when using backend validation layers (crbug.com/1208253)
                "--timeout-ms=30000",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_webgpu_compat_cts",
    tests = {
        "webgpu_cts_compat_tests": targets.legacy_test_config(
            telemetry_test_name = "webgpu_compat_cts",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_telemetry_cts",
            ],
            args = [
                "--extra-browser-args=--use-angle=gl --use-webgpu-adapter=opengles --enable-webgpu-developer-features",
            ],
            swarming = targets.swarming(
                shards = 14,
            ),
            android_swarming = targets.swarming(
                shards = 36,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_webgpu_cts",
    tests = {
        "webgpu_cts_tests": targets.legacy_test_config(
            telemetry_test_name = "webgpu_cts",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            swarming = targets.swarming(
                shards = 14,
            ),
            android_swarming = targets.swarming(
                shards = 36,
            ),
        ),
        "webgpu_cts_with_validation_tests": targets.legacy_test_config(
            telemetry_test_name = "webgpu_cts",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            args = [
                "--enable-dawn-backend-validation",
            ],
            swarming = targets.swarming(
                shards = 14,
            ),
            android_swarming = targets.swarming(
                shards = 36,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_webgpu_cts_asan",
    tests = {
        "webgpu_cts_tests": targets.legacy_test_config(
            telemetry_test_name = "webgpu_cts",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            swarming = targets.swarming(
                shards = 8,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_webgpu_cts_dxc",
    tests = {
        "webgpu_cts_dxc_tests": targets.legacy_test_config(
            telemetry_test_name = "webgpu_cts",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            args = [
                "--use-dxc",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 14,
            ),
        ),
        "webgpu_cts_dxc_with_validation_tests": targets.legacy_test_config(
            telemetry_test_name = "webgpu_cts",
            mixins = [
                "has_native_resultdb_integration",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            args = [
                "--enable-dawn-backend-validation",
                "--use-dxc",
            ],
            ci_only = True,
            swarming = targets.swarming(
                shards = 14,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_default_and_optional_win_media_foundation_specific_gtests",
    tests = {
        # MediaFoundation browser tests, which currently only run on Windows OS,
        # and require physical hardware.
        "media_foundation_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            args = [
                "--gtest_filter=MediaFoundationEncryptedMediaTest*",
                "--use-gpu-in-tests",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_default_and_optional_win_specific_gtests",
    tests = {
        "xr_browser_tests": targets.legacy_test_config(
            test = "xr_browser_tests",
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

targets.legacy_basic_suite(
    name = "gpu_desktop_specific_gtests",
    tests = {
        "tab_capture_end2end_tests": targets.legacy_test_config(
            test = "browser_tests",
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

targets.legacy_basic_suite(
    name = "gpu_fyi_and_optional_non_linux_gtests",
    tests = {
        # gpu_unittests is killing the Swarmed Linux GPU bots similarly to
        # how content_unittests was: http://crbug.com/763498 .
        "gpu_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "gpu_fyi_and_optional_win_specific_gtests",
    tests = {
        "gles2_conform_d3d9_test": targets.legacy_test_config(
            test = "gles2_conform_test",
            args = [
                "--use-gpu-in-tests",
                "--use-angle=d3d9",
            ],
        ),
        "gles2_conform_gl_test": targets.legacy_test_config(
            test = "gles2_conform_test",
            args = [
                "--use-gpu-in-tests",
                "--use-angle=gl",
                "--disable-gpu-sandbox",
            ],
        ),
        # WebNN DirectML backend unit tests, which currently only run on
        # Windows OS, and require physical hardware.
        "services_webnn_unittests": targets.legacy_test_config(
            test = "services_unittests",
            args = [
                "--gtest_filter=WebNN*",
                "--use-gpu-in-tests",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_fyi_mac_specific_gtests",
    tests = {
        # Face and barcode detection unit tests, which currently only run on
        # Mac OS, and require physical hardware.
        "services_unittests": targets.legacy_test_config(
            args = [
                "--gtest_filter=*Detection*",
                "--use-gpu-in-tests",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_fyi_vulkan_swiftshader_gtests",
    tests = {
        "vulkan_swiftshader_content_browsertests": targets.legacy_test_config(
            test = "content_browsertests",
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

targets.legacy_basic_suite(
    name = "gpu_gl_passthrough_ganesh_telemetry_tests",
    tests = {
        "context_lost_gl_passthrough_ganesh_tests": targets.legacy_test_config(
            telemetry_test_name = "context_lost",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=gl --disable-features=SkiaGraphite",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
        ),
        "expected_color_pixel_gl_passthrough_ganesh_test": targets.legacy_test_config(
            telemetry_test_name = "expected_color",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=gl --disable-features=SkiaGraphite",
            ],
        ),
        "gpu_process_launch_tests": targets.legacy_test_config(
            telemetry_test_name = "gpu_process",
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
        "hardware_accelerated_feature_tests": targets.legacy_test_config(
            telemetry_test_name = "hardware_accelerated_feature",
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
        "pixel_skia_gold_gl_passthrough_ganesh_test": targets.legacy_test_config(
            telemetry_test_name = "pixel",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=gl --disable-features=SkiaGraphite",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
        ),
        "screenshot_sync_gl_passthrough_ganesh_tests": targets.legacy_test_config(
            telemetry_test_name = "screenshot_sync",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=gl --disable-features=SkiaGraphite",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_gles2_conform_gtests",
    tests = {
        # The gles2_conform_tests are closed-source and deliberately only
        # run on the FYI waterfall and the optional tryservers.
        "gles2_conform_test": targets.legacy_test_config(
            args = [
                "--use-gpu-in-tests",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_info_collection_telemetry_tests",
    tests = {
        "info_collection_tests": targets.legacy_test_config(
            telemetry_test_name = "info_collection",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "$$MAGIC_SUBSTITUTION_GPUExpectedVendorId",
                "$$MAGIC_SUBSTITUTION_GPUExpectedDeviceId",
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--force_high_performance_gpu",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_mediapipe_passthrough_telemetry_tests",
    tests = {
        "mediapipe_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "mediapipe",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--extra-browser-args=--force_higher_performance_gpu --use-cmd-decoder=passthrough --use-gl=angle",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_mediapipe_validating_telemetry_tests",
    tests = {
        "mediapipe_validating_tests": targets.legacy_test_config(
            telemetry_test_name = "mediapipe",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--extra-browser-args=--force_higher_performance_gpu --use-cmd-decoder=validating",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_metal_passthrough_ganesh_telemetry_tests",
    tests = {
        "context_lost_metal_passthrough_ganesh_tests": targets.legacy_test_config(
            telemetry_test_name = "context_lost",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --disable-features=SkiaGraphite",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
        ),
        "expected_color_pixel_metal_passthrough_ganesh_test": targets.legacy_test_config(
            telemetry_test_name = "expected_color",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --disable-features=SkiaGraphite",
            ],
        ),
        "gpu_process_launch_tests": targets.legacy_test_config(
            telemetry_test_name = "gpu_process",
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
        "hardware_accelerated_feature_tests": targets.legacy_test_config(
            telemetry_test_name = "hardware_accelerated_feature",
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
        "pixel_skia_gold_metal_passthrough_ganesh_test": targets.legacy_test_config(
            telemetry_test_name = "pixel",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --disable-features=SkiaGraphite",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
        ),
        "screenshot_sync_metal_passthrough_ganesh_tests": targets.legacy_test_config(
            telemetry_test_name = "screenshot_sync",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --disable-features=SkiaGraphite",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_metal_passthrough_graphite_telemetry_tests",
    tests = {
        "context_lost_metal_passthrough_graphite_tests": targets.legacy_test_config(
            telemetry_test_name = "context_lost",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --enable-features=SkiaGraphite",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
        ),
        "expected_color_pixel_metal_passthrough_graphite_test": targets.legacy_test_config(
            telemetry_test_name = "expected_color",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --enable-features=SkiaGraphite",
            ],
        ),
        "gpu_process_launch_tests": targets.legacy_test_config(
            telemetry_test_name = "gpu_process",
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
        "hardware_accelerated_feature_tests": targets.legacy_test_config(
            telemetry_test_name = "hardware_accelerated_feature",
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
        "pixel_skia_gold_metal_passthrough_graphite_test": targets.legacy_test_config(
            telemetry_test_name = "pixel",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --enable-features=SkiaGraphite",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
        ),
        "screenshot_sync_metal_passthrough_graphite_tests": targets.legacy_test_config(
            telemetry_test_name = "screenshot_sync",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --enable-features=SkiaGraphite",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_noop_sleep_telemetry_test",
    tests = {
        # The former GPU-specific generator script contained logic to
        # detect whether the so-called "experimental" GPU bots, which test
        # newer driver versions, were identical to the "stable" versions
        # of the bots, and if so to mirror their configurations. We prefer
        # to keep this new script simpler and to just configure this by
        # hand in waterfalls.pyl.
        "noop_sleep_tests": targets.legacy_test_config(
            telemetry_test_name = "noop_sleep",
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_passthrough_telemetry_tests",
    tests = {
        "context_lost_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "context_lost",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                # TODO(crbug.com/1093085): Remove this once we fix the tests.
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "expected_color_pixel_passthrough_test": targets.legacy_test_config(
            telemetry_test_name = "expected_color",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle",
            ],
            android_args = [
                "--extra-browser-args=--force-online-connection-state-for-indicator",
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "gpu_process_launch_tests": targets.legacy_test_config(
            telemetry_test_name = "gpu_process",
            mixins = [
                "has_native_resultdb_integration",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "hardware_accelerated_feature_tests": targets.legacy_test_config(
            telemetry_test_name = "hardware_accelerated_feature",
            mixins = [
                "has_native_resultdb_integration",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "pixel_skia_gold_passthrough_test": targets.legacy_test_config(
            telemetry_test_name = "pixel",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                # TODO(crbug.com/1093085): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "screenshot_sync_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "screenshot_sync",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle",
            ],
            android_args = [
                # TODO(crbug.com/1093085): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_pixel_passthrough_telemetry_tests",
    tests = {
        "expected_color_pixel_passthrough_test": targets.legacy_test_config(
            telemetry_test_name = "expected_color",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
        ),
        "pixel_skia_gold_passthrough_test": targets.legacy_test_config(
            telemetry_test_name = "pixel",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_skia_renderer_vulkan_passthrough_telemetry_tests",
    tests = {
        "vulkan_pixel_skia_gold_test": targets.legacy_test_config(
            telemetry_test_name = "pixel",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-vulkan=native --disable-vulkan-fallback-to-gl-for-testing --enable-features=Vulkan --use-gl=angle --use-angle=gl --use-cmd-decoder=passthrough",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                # TODO(crbug.com/1093085): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_validating_telemetry_tests",
    tests = {
        "context_lost_validating_tests": targets.legacy_test_config(
            telemetry_test_name = "context_lost",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=validating",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "expected_color_pixel_validating_test": targets.legacy_test_config(
            telemetry_test_name = "expected_color",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=validating",
            ],
            android_args = [
                # TODO(crbug.com/1093085): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "gpu_process_launch_tests": targets.legacy_test_config(
            telemetry_test_name = "gpu_process",
            mixins = [
                "has_native_resultdb_integration",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "hardware_accelerated_feature_tests": targets.legacy_test_config(
            telemetry_test_name = "hardware_accelerated_feature",
            mixins = [
                "has_native_resultdb_integration",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "pixel_skia_gold_validating_test": targets.legacy_test_config(
            telemetry_test_name = "pixel",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=validating",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                # TODO(crbug.com/1093085): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
        "screenshot_sync_validating_tests": targets.legacy_test_config(
            telemetry_test_name = "screenshot_sync",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=validating",
            ],
            android_args = [
                # TODO(crbug.com/1093085): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_vulkan_gtests",
    tests = {
        "vulkan_tests": targets.legacy_test_config(
            desktop_args = [
                "--use-gpu-in-tests",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webcodecs_telemetry_test",
    tests = {
        "webcodecs_tests": targets.legacy_test_config(
            telemetry_test_name = "webcodecs",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            # TODO(https://crbug.com/1359405): having --xvfb and --no-xvfb is confusing.
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webcodecs_validating_telemetry_test",
    tests = {
        "webcodecs_tests": targets.legacy_test_config(
            telemetry_test_name = "webcodecs",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=validating",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            # TODO(https://crbug.com/1359405): having --xvfb and --no-xvfb is confusing.
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
    tests = {
        "webgl2_conformance_d3d11_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl2_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--webgl-conformance-version=2.0.1",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=d3d11 --use-cmd-decoder=passthrough --force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            swarming = targets.swarming(
                # These tests currently take about an hour and fifteen minutes
                # to run. Split them into roughly 5-minute shards.
                shards = 20,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
    tests = {
        "webgl2_conformance_gl_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl2_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--webgl-conformance-version=2.0.1",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=gl --use-cmd-decoder=passthrough --force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
            swarming = targets.swarming(
                # These tests currently take about an hour and fifteen minutes
                # to run. Split them into roughly 5-minute shards.
                shards = 20,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
    tests = {
        "webgl2_conformance_gles_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl2_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--webgl-conformance-version=2.0.1",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=gles --use-cmd-decoder=passthrough --force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            swarming = targets.swarming(
                # These tests currently take about an hour and fifteen minutes
                # to run. Split them into roughly 5-minute shards.
                shards = 20,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl2_conformance_metal_passthrough_telemetry_tests",
    tests = {
        "webgl2_conformance_metal_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl2_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--webgl-conformance-version=2.0.1",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=metal --use-cmd-decoder=passthrough --enable-features=EGLDualGPURendering,ForceHighPerformanceGPUForWebGL",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
                "--enable-metal-debug-layers",
            ],
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl2_conformance_validating_telemetry_tests",
    tests = {
        "webgl2_conformance_validating_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl2_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--webgl-conformance-version=2.0.1",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-cmd-decoder=validating --force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
            swarming = targets.swarming(
                # These tests currently take about an hour and fifteen minutes
                # to run. Split them into roughly 5-minute shards.
                shards = 20,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_d3d11_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl1_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=d3d11 --use-cmd-decoder=passthrough --force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_d3d9_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl1_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=d3d9 --use-cmd-decoder=passthrough --force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_gl_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl1_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=gl --use-cmd-decoder=passthrough --force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_gles_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl1_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=gles --use-cmd-decoder=passthrough --force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
            swarming = targets.swarming(
                shards = 6,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_metal_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_metal_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl1_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=metal --use-cmd-decoder=passthrough --enable-features=EGLDualGPURendering,ForceHighPerformanceGPUForWebGL",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
                "--enable-metal-debug-layers",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    tests = {
        "webgl_conformance_swangle_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl1_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=swiftshader --use-cmd-decoder=passthrough --force_high_performance_gpu",
                # We are only interested in running a 'smoketest' to test swangle
                # integration, not the full conformance suite.
                "--test-filter=conformance/rendering/gl-drawelements.html",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_swangle_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_swangle_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl1_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--extra-browser-args=--use-gl=angle --use-angle=swiftshader --use-cmd-decoder=passthrough",
                "--xvfb",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_telemetry_tests",
    tests = {
        "webgl_conformance_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl1_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
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
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_validating_telemetry_tests",
    tests = {
        "webgl_conformance_validating_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl1_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-cmd-decoder=validating --force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            android_args = [
                "$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices",
            ],
            chromeos_args = [
                "$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote",
            ],
            lacros_args = [
                "--extra-browser-args=--enable-features=UseOzonePlatform --ozone-platform=wayland",
                "--xvfb",
                "--no-xvfb",
                "--use-weston",
                "--weston-use-gl",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
            android_swarming = targets.swarming(
                shards = 6,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_vulkan_passthrough_tests": targets.legacy_test_config(
            telemetry_test_name = "webgl1_conformance",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-angle=vulkan --use-cmd-decoder=passthrough --force_high_performance_gpu",
                "$$MAGIC_SUBSTITUTION_GPUParallelJobs",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "headless_browser_gtests",
    tests = {
        "headless_browsertests": None,
        "headless_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "ios_blink_tests",
    tests = {
        "absl_hardening_tests": None,
        "angle_unittests": targets.legacy_test_config(
            use_isolated_scripts_api = True,
        ),
        "base_unittests": None,
        "blink_common_unittests": None,
        "blink_fuzzer_unittests": None,
        "blink_heap_unittests": None,
        "blink_platform_unittests": None,
        "blink_unittests": None,
        "blink_unittests_v2": None,
        "boringssl_crypto_tests": None,
        "boringssl_ssl_tests": None,
        "capture_unittests": None,
        "cast_unittests": None,
        "cc_unittests": targets.legacy_test_config(
            test = "cc_unittests",
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.cc_unittests.filter",
                "--use-gpu-in-tests",
            ],
        ),
        "components_browsertests": None,
        "components_unittests": targets.legacy_test_config(
            test = "components_unittests",
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.use_blink.components_unittests.filter",
            ],
        ),
        "compositor_unittests": targets.legacy_test_config(
            test = "compositor_unittests",
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.compositor_unittests.filter",
            ],
        ),
        "content_browsertests": targets.legacy_test_config(
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.content_browsertests.filter",
            ],
            swarming = targets.swarming(
                shards = 8,
                expiration_sec = 10800,
                hard_timeout_sec = 14400,
            ),
            timeout_sec = 14400,
        ),
        "content_unittests": None,
        "crashpad_tests": None,
        "crypto_unittests": None,
        "device_unittests": None,
        "display_unittests": None,
        "env_chromium_unittests": None,
        "events_unittests": None,
        "gcm_unit_tests": None,
        "gfx_unittests": None,
        "gin_unittests": None,
        "gl_unittests": None,
        "google_apis_unittests": None,
        "gpu_unittests": None,
        "gwp_asan_unittests": None,
        "ipc_tests": None,
        "latency_unittests": None,
        "leveldb_unittests": None,
        "libjingle_xmpp_unittests": None,
        "liburlpattern_unittests": None,
        "media_unittests": None,
        "media_unittests_skia_graphite_dawn": targets.legacy_test_config(
            test = "media_unittests",
            args = [
                "--test-launcher-bot-mode",
                "--enable-features=SkiaGraphite",
                "--skia-graphite-backend=dawn",
                "--use-gpu-in-tests",
            ],
        ),
        "media_unittests_skia_graphite_metal": targets.legacy_test_config(
            test = "media_unittests",
            args = [
                "--test-launcher-bot-mode",
                "--enable-features=SkiaGraphite",
                "--skia-graphite-backend=metal",
                "--use-gpu-in-tests",
            ],
        ),
        "midi_unittests": None,
        "mojo_unittests": None,
        "net_unittests": None,
        "perfetto_unittests": None,
        "printing_unittests": None,
        "services_unittests": None,
        "shell_dialogs_unittests": None,
        "skia_unittests": None,
        "sql_unittests": None,
        "storage_unittests": None,
        "ui_base_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=testing/buildbot/filters/ios.ui_base_unittests.filter",
            ],
        ),
        "ui_touch_selection_unittests": None,
        "ui_unittests": None,
        "url_unittests": None,
        "viz_unittests": targets.legacy_test_config(
            test = "viz_unittests",
            args = [
                "--test-launcher-bot-mode",
                "--test-launcher-filter-file=testing/buildbot/filters/ios.viz_unittests.filter",
                "--use-gpu-in-tests",
            ],
        ),
        "wtf_unittests": None,
        "zlib_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "ios_common_tests",
    tests = {
        "absl_hardening_tests": None,
        "boringssl_crypto_tests": None,
        "boringssl_ssl_tests": None,
        "crashpad_tests": None,
        "crypto_unittests": None,
        "google_apis_unittests": None,
        "ios_components_unittests": None,
        "ios_net_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "ios_remoting_unittests": None,
        "ios_testing_unittests": None,
        "net_unittests": None,
        # TODO(https://bugs.chromium.org/p/gn/issues/detail?id=340): Enable this.
        # "rust_gtest_interop_unittests": None,
        "services_unittests": None,
        "sql_unittests": None,
        "url_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "ios_crash_xcuitests",
    tests = {
        "ios_crash_xcuitests_module": None,
    },
)

targets.legacy_basic_suite(
    name = "ios_eg2_cq_tests",
    tests = {
        "ios_chrome_integration_eg2tests_module": targets.legacy_test_config(
            mixins = [
                "ios_parallel_simulators",
            ],
            swarming = targets.swarming(
                shards = 8,
            ),
        ),
        "ios_web_shell_eg2tests_module": None,
    },
)

targets.legacy_basic_suite(
    name = "ios_eg2_tests",
    tests = {
        "ios_chrome_bookmarks_eg2tests_module": None,
        "ios_chrome_settings_eg2tests_module": targets.legacy_test_config(
            mixins = [
                "ios_parallel_simulators",
            ],
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
        "ios_chrome_signin_eg2tests_module": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 6,
            ),
        ),
        "ios_chrome_smoke_eg2tests_module": None,
        "ios_chrome_ui_eg2tests_module": targets.legacy_test_config(
            mixins = [
                "ios_parallel_simulators",
            ],
            swarming = targets.swarming(
                shards = 12,
            ),
        ),
        "ios_chrome_web_eg2tests_module": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "ios_showcase_eg2tests_module": None,
    },
)

targets.legacy_basic_suite(
    name = "ios_remoting_fyi_unittests",
    tests = {
        "ios_remoting_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "ios_screen_size_dependent_tests",
    tests = {
        "base_unittests": None,
        "components_unittests": None,
        "gfx_unittests": None,
        "ios_chrome_unittests": None,
        "ios_web_inttests": None,
        "ios_web_unittests": None,
        "ios_web_view_inttests": None,
        "ios_web_view_unittests": None,
        "skia_unittests": None,
        "ui_base_unittests": None,
    },
)

# END tests which run on the GPU bots

targets.legacy_basic_suite(
    name = "js_code_coverage_browser_tests",
    tests = {
        "js_code_coverage_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            swarming = targets.swarming(
                shards = 16,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "lacros_all_tast_tests",
    tests = {
        "lacros_all_tast_tests": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/923426#c27
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "lacros_all_tast_tests_informational",
    tests = {
        "lacros_all_tast_tests_informational": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/923426#c27
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "lacros_cq_tast_tests_eve",
    tests = {
        "lacros_cq_tast_tests_eve": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/923426#c27
            ),
            experiment_percentage = 5,
        ),
    },
)

targets.legacy_basic_suite(
    name = "lacros_device_or_vm_gtests",
    tests = {
        "cc_unittests": None,
        "ozone_unittests": None,
        "vaapi_unittest": targets.legacy_test_config(
            args = [
                "--stop-ui",
                # Tell libva to do dummy encoding/decoding. For more info, see:
                # https://github.com/intel/libva/blob/master/va/va_fool.c#L47
                "--env-var",
                "LIBVA_DRIVERS_PATH",
                "./",
                "--env-var",
                "LIBVA_DRIVER_NAME",
                "libfake",
                "--gtest_filter=\"VaapiTest.*\"",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "lacros_fyi_tast_tests",
    tests = {
        "lacros_fyi_tast_tests": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/923426#c27
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "lacros_skylab_amd64_fyi",
    tests = {
        "lacros_fyi_tast_tests": targets.legacy_test_config(
            tast_expr = "(\"group:mainline\" && \"dep:lacros\" && !informational)",
            timeout_sec = 10800,
        ),
        "ozone_unittests": targets.legacy_test_config(
            timeout_sec = 3600,
        ),
    },
)

# create this temporary lacros arm test suites that runs on skylab
# TODO(crbug.com/1247425): remove it if it is the same as
# lacros_skylab
targets.legacy_basic_suite(
    name = "lacros_skylab_arm_tests_fyi",
    tests = {
        "lacros_all_tast_tests": targets.legacy_test_config(
            tast_expr = "(\"group:mainline\" && \"dep:lacros\" && !informational)",
            timeout_sec = 10800,
        ),
        "ozone_unittests": targets.legacy_test_config(
            timeout_sec = 3600,
        ),
        "viz_unittests": targets.legacy_test_config(
            timeout_sec = 3600,
        ),
    },
)

# Lacros tests that run on Skylab, and these tests are usually HW sensative,
# Currently we only run Tast tests.
targets.legacy_basic_suite(
    name = "lacros_skylab_tests",
    tests = {
        "lacros_all_tast_tests": targets.legacy_test_config(
            tast_expr = "(\"group:mainline\" && (\"dep:lacros_stable\" || \"dep:lacros\") && !informational)",
            test_level_retries = 2,
            mixins = [
                "has_native_resultdb_integration",
            ],
            timeout_sec = 10800,
            shards = 2,
        ),
    },
)

# This target should usually be the same as `lacros_skylab_tests`. We use
# a different target for version skew so we can easily disable all version skew
# tests during an outage.
targets.legacy_basic_suite(
    name = "lacros_skylab_tests_version_skew",
    tests = {
        "lacros_all_tast_tests": targets.legacy_test_config(
            tast_expr = "(\"group:mainline\" && (\"dep:lacros_stable\" || \"dep:lacros\") && !informational)",
            test_level_retries = 2,
            mixins = [
                "has_native_resultdb_integration",
            ],
            timeout_sec = 10800,
            # TODO(crbug.com/1499803) re-enable when we have the required OS versions.
            experiment_percentage = 100,
            shards = 2,
        ),
    },
)

targets.legacy_basic_suite(
    name = "lacros_skylab_tests_with_gtests",
    tests = {
        "chromeos_integration_tests": None,
    },
)

targets.legacy_basic_suite(
    name = "lacros_vm_gtests",
    tests = {
        "base_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "leak_detection_isolated_scripts",
    tests = {
        "memory.leak_detection": targets.legacy_test_config(
            test = "performance_test_suite",
            args = [
                "--pageset-repeat=1",
                "--test-shard-map-filename=linux_leak_detection_shard_map.json",
                "--upload-results",
                "--output-format=histograms",
                "--browser=release",
                "--xvfb",
            ],
            swarming = targets.swarming(
                shards = 10,
                expiration_sec = 36000,
                hard_timeout_sec = 10800,
                io_timeout_sec = 3600,
            ),
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
                args = [
                    "--smoke-test-mode",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "linux_cfm_gtests",
    tests = {
        "chromeos_unittests": None,
        "unit_tests": None,
    },
)

targets.legacy_basic_suite(
    name = "linux_chromeos_browser_tests_require_lacros",
    tests = {
        "browser_tests_require_lacros": targets.legacy_test_config(
            test = "browser_tests",
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/linux-chromeos.browser_tests.require_lacros.filter",
                "--lacros-chrome-path=lacros_clang_x64/test_lacros_chrome",
            ],
            swarming = targets.swarming(
                shards = 8,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "linux_chromeos_lacros_gtests",
    tests = {
        # Chrome OS (Ash) and Lacros only.
        "chromeos_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "linux_chromeos_oobe_specific_tests",
    tests = {
        # TODO(crbug.com/1071693): Merge this suite back in to the main
        # browser_tests when the tests no longer fail on MSAN.
        "oobe_only_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.msan.browser_tests.oobe_positive.filter",
            ],
            swarming = targets.swarming(
                shards = 20,
            ),
            experiment_percentage = 100,
        ),
    },
)

targets.legacy_basic_suite(
    name = "linux_chromeos_specific_gtests",
    tests = {
        # Chrome OS only.
        "ash_components_unittests": None,
        # TODO(crbug.com/1351793) Enable on CQ when stable.
        "ash_crosapi_tests": targets.legacy_test_config(
            ci_only = True,
        ),
        "ash_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
        "ash_webui_unittests": None,
        "aura_unittests": None,
        "chromeos_components_unittests": None,
        "exo_unittests": None,
        "gl_unittests_ozone": None,
        "keyboard_unittests": None,
        "ozone_gl_unittests": targets.legacy_test_config(
            args = [
                "--ozone-platform=headless",
            ],
        ),
        "ozone_unittests": None,
        "ozone_x11_unittests": None,
        "shell_encryption_unittests": None,
        "ui_chromeos_unittests": None,
        "usage_time_limit_unittests": targets.legacy_test_config(
            experiment_percentage = 100,
        ),
        "wayland_client_perftests": None,
        "wayland_client_tests": None,
    },
)

targets.legacy_basic_suite(
    name = "linux_flavor_specific_chromium_gtests",
    tests = {
        # Android, Chrome OS and Linux
        "sandbox_linux_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "linux_force_accessibility_gtests",
    tests = {
        "browser_tests": targets.legacy_test_config(
            args = [
                "--force-renderer-accessibility",
                "--test-launcher-filter-file=../../testing/buildbot/filters/accessibility-linux.browser_tests.filter",
            ],
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "content_browsertests": targets.legacy_test_config(
            args = [
                "--force-renderer-accessibility",
                "--test-launcher-filter-file=../../testing/buildbot/filters/accessibility-linux.content_browsertests.filter",
            ],
            swarming = targets.swarming(
                shards = 8,
            ),
        ),
        "interactive_ui_tests": targets.legacy_test_config(
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

targets.legacy_basic_suite(
    name = "linux_lacros_chrome_browsertests_non_version_skew",
    tests = {
        "lacros_chrome_browsertests": targets.legacy_test_config(
            test = "lacros_chrome_browsertests",
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/linux-lacros.lacros_chrome_browsertests.filter",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "linux_lacros_chrome_browsertests_version_skew",
    tests = {
        "lacros_chrome_browsertests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/linux-lacros.lacros_chrome_browsertests.filter;../../testing/buildbot/filters/linux-lacros.lacros_chrome_browsertests.skew.filter",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "linux_lacros_chrome_interactive_ui_tests_version_skew",
    tests = {
        "interactive_ui_tests": targets.legacy_test_config(
            test = "interactive_ui_tests",
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/linux-lacros.interactive_ui_tests.filter;../../testing/buildbot/filters/linux-lacros.interactive_ui_tests.skew.filter",
            ],
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "linux_lacros_specific_gtests",
    tests = {
        "lacros_chrome_unittests": None,
        "ozone_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "linux_specific_chromium_isolated_scripts",
    tests = {
        # not_site_per_process_blink_web_tests provides coverage for
        # running Layout Tests without site-per-process.  This is the mode used
        # on Android and Android bots currently do not run the full set of
        # layout tests.  Running in this mode on linux compensates for lack of
        # direct Android coverage.
        "not_site_per_process_blink_web_tests": targets.legacy_test_config(
            test = "blink_web_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                "--flag-specific=disable-site-isolation-trials",
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 8,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
        # not_site_per_process_blink_wpt_tests provides coverage for
        # running WPTs without site-per-process.  This is the mode used
        # on Android and Android bots currently do not run the full set of
        # layout tests.  Running in this mode on linux compensates for lack of
        # direct Android coverage.
        "not_site_per_process_blink_wpt_tests": targets.legacy_test_config(
            test = "blink_wpt_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                "--flag-specific=disable-site-isolation-trials",
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
            ],
            swarming = targets.swarming(
                shards = 10,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
        "webdriver_wpt_tests": targets.legacy_test_config(
            test = "chrome_wpt_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--test-type=wdspec",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "linux_specific_xr_gtests",
    tests = {
        "xr_browser_tests": targets.legacy_test_config(
            test = "xr_browser_tests",
        ),
    },
)

# TODO(crbug.com/1320449): Remove this set of test suites when LSan can be
# enabled Mac ASan bots. This list will be gradually filled with more tests
# until the bot has parity with ASan bots, and the ASan bot can then enable
# LSan and the mac-lsan-fyi-rel bot go away.
targets.legacy_basic_suite(
    name = "mac_lsan_fyi_gtests",
    tests = {
        "absl_hardening_tests": None,
        "accessibility_unittests": None,
        "app_shell_unittests": None,
        "base_unittests": None,
        "blink_heap_unittests": None,
        "blink_platform_unittests": None,
        "blink_unittests": None,
        "blink_unittests_v2": None,
        "cc_unittests": None,
        "components_unittests": None,
        "content_unittests": None,
        "crashpad_tests": None,
        "cronet_unittests": None,
        "device_unittests": None,
        "net_unittests": None,
        # TODO(crbug.com/1459686): Enable this.
        # "rust_gtest_interop_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "mac_specific_chromium_gtests",
    tests = {
        "power_sampler_unittests": None,
        "sandbox_unittests": None,
        "updater_tests": None,
        "xr_browser_tests": targets.legacy_test_config(
            test = "xr_browser_tests",
        ),
    },
)

targets.legacy_basic_suite(
    name = "mac_specific_isolated_scripts",
    tests = {
        "mac_signing_tests": None,
    },
)

targets.legacy_basic_suite(
    name = "minidump_uploader_tests",
    tests = {
        "minidump_uploader_test": targets.legacy_test_config(
            experiment_percentage = 100,
        ),
    },
)

targets.legacy_basic_suite(
    name = "model_validation_tests",
    tests = {
        "model_validation_tests": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--out_dir=.",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "mojo_python_unittests_isolated_scripts",
    tests = {
        "mojo_python_unittests": targets.legacy_test_config(
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "monochrome_public_apk_checker_isolated_script",
    tests = {
        "monochrome_public_apk_checker": targets.legacy_test_config(
            remove_mixins = [
                "android_r",
                "bullhead",
                "flame",
                "marshmallow",
                "mdarcy",
                "oreo_fleet",
                "oreo_mr1_fleet",
                "pie_fleet",
                "walleye",
            ],
            swarming = targets.swarming(
                dimensions = {
                    "os": "Ubuntu-18.04",
                    "cpu": "x86-64",
                    "device_os": None,
                    "device_os_flavor": None,
                    "device_playstore_version": None,
                    "device_type": None,
                },
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "network_sandbox_browser_tests",
    tests = {
        "browser_tests_network_sandbox": targets.legacy_test_config(
            test = "browser_tests",
            args = [
                "--enable-features=NetworkServiceSandbox",
            ],
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "components_browsertests_network_sandbox": targets.legacy_test_config(
            test = "components_browsertests",
            args = [
                "--enable-features=NetworkServiceSandbox",
            ],
        ),
        "content_browsertests_network_sandbox": targets.legacy_test_config(
            test = "content_browsertests",
            args = [
                "--enable-features=NetworkServiceSandbox",
            ],
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "extensions_browsertests_network_sandbox": targets.legacy_test_config(
            test = "extensions_browsertests",
            args = [
                "--enable-features=NetworkServiceSandbox",
            ],
        ),
        "interactive_ui_tests_network_sandbox": targets.legacy_test_config(
            test = "interactive_ui_tests",
            args = [
                "--enable-features=NetworkServiceSandbox",
            ],
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "sync_integration_tests_network_sandbox": targets.legacy_test_config(
            test = "sync_integration_tests",
            args = [
                "--enable-features=NetworkServiceSandbox",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "network_service_fyi_gtests",
    tests = {
        "network_service_web_request_proxy_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            args = [
                "--enable-features=ForceWebRequestProxyForTest",
            ],
            swarming = targets.swarming(
                shards = 15,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "non_android_and_cast_and_chromeos_chromium_gtests",
    tests = {
        "cronet_tests": None,
        "cronet_unittests": None,
        "headless_browsertests": None,
        "headless_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "non_android_chromium_gtests",
    tests = {
        "accessibility_unittests": None,
        "app_shell_unittests": None,
        "blink_fuzzer_unittests": None,
        "browser_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "chrome_app_unittests": None,
        "chromedriver_unittests": None,
        "extensions_browsertests": None,
        "extensions_unittests": None,
        "filesystem_service_unittests": None,  # https://crbug.com/862712
        "interactive_ui_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "message_center_unittests": None,
        "nacl_loader_unittests": None,
        "native_theme_unittests": None,
        "pdf_unittests": None,
        "ppapi_unittests": None,
        "printing_unittests": None,
        "remoting_unittests": None,
        "snapshot_unittests": None,
        "sync_integration_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "ui_unittests": None,
        "views_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "non_android_chromium_gtests_no_nacl",
    tests = {
        "accessibility_unittests": None,
        "app_shell_unittests": None,
        "blink_fuzzer_unittests": None,
        "browser_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "chrome_app_unittests": None,
        "chromedriver_unittests": None,
        "extensions_browsertests": None,
        "extensions_unittests": None,
        "filesystem_service_unittests": None,  # https://crbug.com/862712
        "interactive_ui_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "message_center_unittests": None,
        "native_theme_unittests": None,
        "pdf_unittests": None,
        "printing_unittests": None,
        "remoting_unittests": None,
        "snapshot_unittests": None,
        "sync_integration_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "ui_unittests": None,
        "views_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "non_android_chromium_gtests_skia_gold",
    tests = {
        "views_examples_unittests": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "optimization_guide_android_gtests",
    tests = {
        "optimization_guide_components_unittests": targets.legacy_test_config(
            test = "components_unittests",
            args = [
                "--gtest_filter=*OptimizationGuide*:*PageEntities*:*EntityAnnotator*",
            ],
        ),
        # TODO(mgeorgaklis): Add optimization_guide_unittests when they become Android compatible.
    },
)

targets.legacy_basic_suite(
    name = "optimization_guide_gtests",
    tests = {
        "optimization_guide_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            args = [
                "--gtest_filter=*OptimizationGuide*:*PageContentAnnotations*",
            ],
        ),
        "optimization_guide_components_unittests": targets.legacy_test_config(
            test = "components_unittests",
            args = [
                "--gtest_filter=*OptimizationGuide*:*PageEntities*:*EntityAnnotator*",
            ],
        ),
        "optimization_guide_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "perfetto_gtests",
    tests = {
        "base_unittests": None,
        "browser_tests": targets.legacy_test_config(
            args = [
                "--gtest_filter=ChromeTracingDelegateBrowserTest.*",
            ],
        ),
        "content_browsertests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 8,
            ),
            android_swarming = targets.swarming(
                shards = 15,
            ),
        ),
        "perfetto_unittests": None,
        "services_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "perfetto_gtests_android",
    tests = {
        "android_browsertests": targets.legacy_test_config(
            args = [
                "--gtest_filter=StartupMetricsTest.*",
            ],
        ),
        "base_unittests": None,
        "content_browsertests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 8,
            ),
            android_swarming = targets.swarming(
                shards = 15,
            ),
        ),
        "perfetto_unittests": None,
        "services_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "performance_smoke_test_isolated_scripts",
    tests = {
        "performance_test_suite": targets.legacy_test_config(
            args = [
                "--pageset-repeat=1",
                "--test-shard-map-filename=smoke_test_benchmark_shard_map.json",
            ],
            swarming = targets.swarming(
                shards = 2,
                hard_timeout_sec = 960,
            ),
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
                args = [
                    "--smoke-test-mode",
                ],
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "pixel_browser_tests_gtests",
    tests = {
        "pixel_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            mixins = [
                "skia_gold_test",
            ],
            args = [
                "--browser-ui-tests-verify-pixels",
                "--enable-pixel-output-in-tests",
                "--test-launcher-filter-file=../../testing/buildbot/filters/pixel_tests.filter",
                "--test-launcher-jobs=1",
            ],
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "pixel_interactive_ui_tests": targets.legacy_test_config(
            test = "interactive_ui_tests",
            mixins = [
                "skia_gold_test",
            ],
            args = [
                "--browser-ui-tests-verify-pixels",
                "--enable-pixel-output-in-tests",
                "--test-launcher-filter-file=../../testing/buildbot/filters/pixel_tests.filter",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "pixel_experimental_browser_tests_gtests",
    tests = {
        "pixel_experimental_browser_tests": targets.legacy_test_config(
            test = "browser_tests",
            mixins = [
                "skia_gold_test",
            ],
            args = [
                "--browser-ui-tests-verify-pixels",
                "--enable-pixel-output-in-tests",
                "--test-launcher-filter-file=../../testing/buildbot/filters/linux-chromeos.browser_tests.pixel_tests.filter",
            ],
            experiment_percentage = 100,
        ),
    },
)

targets.legacy_basic_suite(
    name = "private_code_failure_test_isolated_scripts",
    tests = {
        "private_code_failure_test": None,
    },
)

# TODO(dpranke): These are run on the p/chromium waterfall; they should
# probably be run on other builders, and we should get rid of the p/chromium
# waterfall.
targets.legacy_basic_suite(
    name = "public_build_scripts",
    tests = {
        "checkbins": targets.legacy_test_config(
            script = "checkbins.py",
        ),
    },
)

targets.legacy_basic_suite(
    name = "pytype_tests",
    tests = {
        "blink_pytype": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
        "fuchsia_pytype": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
        "gold_common_pytype": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
        "gpu_pytype": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
        "testing_pytype": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
        ),
    },
)

# Rust tests run on all targets.
targets.legacy_basic_suite(
    name = "rust_common_gtests",
    tests = {
        "base_unittests": None,
        # For go/rusty-qr-code-generator
        "components_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/rust.components_unittests.filter",
            ],
        ),
        "mojo_rust_integration_unittests": None,
        "mojo_rust_unittests": None,
        "rust_gtest_interop_unittests": None,
        "test_cpp_including_rust_unittests": targets.legacy_test_config(
            test = "test_cpp_including_rust_unittests",
        ),
        "test_serde_json_lenient": targets.legacy_test_config(
            test = "test_serde_json_lenient",
        ),
    },
)

targets.legacy_basic_suite(
    name = "rust_native_tests",
    tests = {
        "build_rust_tests": targets.legacy_test_config(
            test = "build_rust_tests",
        ),
    },
)

targets.legacy_basic_suite(
    name = "site_isolation_android_fyi_gtests",
    tests = {
        "site_per_process_android_browsertests": targets.legacy_test_config(
            test = "android_browsertests",
            args = [
                "--site-per-process",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
        "site_per_process_chrome_public_test_apk": targets.legacy_test_config(
            test = "chrome_public_test_apk",
            mixins = [
                "skia_gold_test",
                "has_native_resultdb_integration",
            ],
            args = [
                "--site-per-process",
            ],
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
        "site_per_process_chrome_public_unit_test_apk": targets.legacy_test_config(
            test = "chrome_public_unit_test_apk",
            mixins = [
                "skia_gold_test",
            ],
            args = [
                "--site-per-process",
            ],
        ),
        "site_per_process_components_browsertests": targets.legacy_test_config(
            test = "components_browsertests",
            args = [
                "--site-per-process",
            ],
        ),
        "site_per_process_components_unittests": targets.legacy_test_config(
            test = "components_unittests",
            args = [
                "--site-per-process",
            ],
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
        "site_per_process_content_browsertests": targets.legacy_test_config(
            test = "content_browsertests",
            args = [
                "--site-per-process",
                "--test-launcher-filter-file=../../testing/buildbot/filters/site_isolation_android.content_browsertests.filter",
            ],
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "site_per_process_content_shell_test_apk": targets.legacy_test_config(
            test = "content_shell_test_apk",
            args = [
                "--site-per-process",
            ],
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "site_per_process_content_unittests": targets.legacy_test_config(
            test = "content_unittests",
            args = [
                "--site-per-process",
            ],
        ),
        "site_per_process_unit_tests": targets.legacy_test_config(
            test = "unit_tests",
            args = [
                "--site-per-process",
            ],
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "swangle_gtests",
    tests = {
        "angle_deqp_egl_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles2_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles31_rotate180_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles31_rotate270_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles31_rotate90_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles31_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            swarming = targets.swarming(
                shards = 10,
            ),
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles3_rotate180_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles3_rotate270_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles3_rotate90_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_gles3_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            swarming = targets.swarming(
                shards = 4,
            ),
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_khr_gles2_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_khr_gles31_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_deqp_khr_gles3_tests": targets.legacy_test_config(
            args = [
                "--use-angle=swiftshader",
            ],
            use_isolated_scripts_api = True,
        ),
        "angle_end2end_tests": targets.legacy_test_config(
            args = [
                "--gtest_filter=*Vulkan_SwiftShader*",
            ],
            use_isolated_scripts_api = True,
        ),
    },
)

targets.legacy_basic_suite(
    name = "system_webview_shell_instrumentation_tests",
    tests = {
        "system_webview_shell_layout_test_apk": None,
    },
)

targets.legacy_basic_suite(
    name = "system_webview_wpt",
    tests = {
        "system_webview_wpt": targets.legacy_test_config(
            results_handler = "layout tests",
            args = [
                "--no-wpt-internal",
            ],
            swarming = targets.swarming(
                shards = 25,
                expiration_sec = 18000,
                hard_timeout_sec = 14400,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "telemetry_android_minidump_unittests_isolated_scripts",
    tests = {
        "telemetry_chromium_minidump_unittests": targets.legacy_test_config(
            test = "telemetry_perf_unittests_android_chrome",
            args = [
                "BrowserMinidumpTest",
                "--browser=android-chromium",
                "-v",
                "--passthrough",
                "--retry-limit=2",
            ],
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "telemetry_monochrome_minidump_unittests": targets.legacy_test_config(
            test = "telemetry_perf_unittests_android_monochrome",
            args = [
                "BrowserMinidumpTest",
                "--browser=android-chromium-monochrome",
                "-v",
                "--passthrough",
                "--retry-limit=2",
            ],
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "telemetry_desktop_minidump_unittests_isolated_scripts",
    tests = {
        # Takes ~2.5 minutes of bot time to run.
        "telemetry_desktop_minidump_unittests": targets.legacy_test_config(
            test = "telemetry_perf_unittests",
            args = [
                "BrowserMinidumpTest",
                "-v",
                "--passthrough",
                "--retry-limit=2",
            ],
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "telemetry_perf_unittests_isolated_scripts",
    tests = {
        "telemetry_perf_unittests": targets.legacy_test_config(
            args = [
                # TODO(crbug.com/1077284): Remove this once Crashpad is the default.
                "--extra-browser-args=--enable-crashpad",
            ],
            swarming = targets.swarming(
                shards = 12,
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "telemetry_perf_unittests_isolated_scripts_android",
    tests = {
        "telemetry_perf_unittests": targets.legacy_test_config(
            test = "telemetry_perf_unittests_android_chrome",
            args = [
                # TODO(crbug.com/1077284): Remove this once Crashpad is the default.
                "--extra-browser-args=--enable-crashpad",
            ],
            swarming = targets.swarming(
                shards = 12,
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "telemetry_perf_unittests_isolated_scripts_xvfb",
    tests = {
        "telemetry_perf_unittests": targets.legacy_test_config(
            args = [
                # TODO(crbug.com/1077284): Remove this once Crashpad is the default.
                "--extra-browser-args=--enable-crashpad",
                "--xvfb",
            ],
            swarming = targets.swarming(
                shards = 12,
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "test_traffic_annotation_auditor_script",
    tests = {
        "test_traffic_annotation_auditor": targets.legacy_test_config(
            script = "test_traffic_annotation_auditor.py",
        ),
    },
)

targets.legacy_basic_suite(
    name = "updater_gtests_linux",
    tests = {
        "updater_tests": targets.legacy_test_config(
            mixins = [
                "updater-default-pool",
            ],
        ),
        # 'updater_tests_system' is not yet supported on Linux.
    },
)

targets.legacy_basic_suite(
    name = "updater_gtests_mac",
    tests = {
        "updater_tests": targets.legacy_test_config(
            mixins = [
                "updater-default-pool",
            ],
        ),
        "updater_tests_system": targets.legacy_test_config(
            mixins = [
                "updater-mac-pool",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "updater_gtests_win",
    tests = {
        "updater_tests": targets.legacy_test_config(
            mixins = [
                "integrity_high",
                "updater-default-pool",
            ],
        ),
        "updater_tests_system": targets.legacy_test_config(
            mixins = [
                "integrity_high",
                "updater-default-pool",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "updater_gtests_win_uac",
    tests = {
        "updater_tests_system": targets.legacy_test_config(
            mixins = [
                "integrity_high",
                "updater-win-uac-pool",
            ],
        ),
        "updater_tests_win_uac": targets.legacy_test_config(
            mixins = [
                "updater-win-uac-pool",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "upload_perfetto",
    tests = {
        "upload_trace_processor": None,
    },
)

targets.legacy_basic_suite(
    name = "variations_smoke_tests",
    tests = {
        "variations_smoke_tests": targets.legacy_test_config(
            test = "variations_smoke_tests",
            mixins = [
                "skia_gold_test",
            ],
            resultdb = targets.resultdb(
                enable = True,
                result_format = "single",
            ),
        ),
    },
)

# Not applicable for android x86 & x64 since the targets here assert
# "enable_vr" in GN which is only true for android arm & arm64.
# For details, see the following files:
#  * //chrome/android/BUILD.gn
#  * //chrome/browser/android/vr/BUILD.gn
#  * //device/vr/buildflags/buildflags.gni
targets.legacy_basic_suite(
    name = "vr_android_specific_chromium_tests",
    tests = {
        "chrome_public_test_vr_apk": targets.legacy_test_config(
            args = [
                "--shared-prefs-file=//chrome/android/shared_preference_files/test/vr_cardboard_skipdon_setupcomplete.json",
                "--additional-apk=//third_party/gvr-android-sdk/test-apks/vr_services/vr_services_current.apk",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "vr_android_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "vr_platform_specific_chromium_gtests",
    tests = {
        # Only run on platforms that intend to support WebVR in the near
        # future.
        "vr_common_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "vulkan_swiftshader_isolated_scripts",
    tests = {
        "vulkan_swiftshader_blink_web_tests": targets.legacy_test_config(
            test = "blink_web_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
                "--skipped=always",
                "--flag-specific=skia-vulkan-swiftshader",
            ],
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "web_engine_gtests",
    tests = {
        "cast_runner_browsertests": None,
        "cast_runner_integration_tests": None,
        "cast_runner_unittests": None,
        "web_engine_browsertests": None,
        "web_engine_integration_tests": None,
        "web_engine_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "webrtc_chromium_baremetal_gtests",
    tests = {
        # Run capture unittests on bots that have real webcams.
        "capture_unittests": targets.legacy_test_config(
            args = [
                "--enable-logging",
                "--v=1",
                "--test-launcher-jobs=1",
                "--test-launcher-print-test-stdio=always",
            ],
            swarming = targets.swarming(
                dimensions = {
                    "pool": "WebRTC-chromium",
                },
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webrtc_chromium_gtests",
    tests = {
        "browser_tests": targets.legacy_test_config(
            args = [
                "--gtest_filter=WebRtcStatsPerfBrowserTest.*:WebRtcVideoDisplayPerfBrowserTests*:WebRtcVideoQualityBrowserTests*:WebRtcVideoHighBitrateBrowserTest*:WebRtcWebcamBrowserTests*",
                "--run-manual",
                "--ui-test-action-max-timeout=300000",
                "--test-launcher-timeout=350000",
                "--test-launcher-jobs=1",
                "--test-launcher-bot-mode",
                "--test-launcher-print-test-stdio=always",
            ],
        ),
        # TODO(b/246519185) - Py3 incompatible, decide if to keep test.:
        # "browser_tests_apprtc": targets.legacy_test_config(
        #     args = [
        #         "--gtest_filter=WebRtcApprtcBrowserTest.*",
        #         "--run-manual",
        #         "--test-launcher-jobs=1",
        #     ],
        # ),
        "browser_tests_functional": targets.legacy_test_config(
            test = "browser_tests",
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/webrtc_functional.browser_tests.filter",
                "--run-manual",
                "--test-launcher-jobs=1",
            ],
        ),
        # Run all normal WebRTC content_browsertests. This is mostly so
        # the FYI bots can detect breakages.
        "content_browsertests": targets.legacy_test_config(
            args = [
                "--gtest_filter=WebRtc*",
            ],
        ),
        # These run a few tests on fake webcams. They need to run sequentially,
        # otherwise tests may interfere with each other.
        "content_browsertests_sequential": targets.legacy_test_config(
            test = "content_browsertests",
            args = [
                "--gtest_filter=UsingRealWebcam*",
                "--run-manual",
                "--test-launcher-jobs=1",
            ],
        ),
        "content_browsertests_stress": targets.legacy_test_config(
            test = "content_browsertests",
            args = [
                "--gtest_filter=WebRtc*MANUAL*:-UsingRealWebcam*",
                "--run-manual",
                "--ui-test-action-max-timeout=110000",
                "--test-launcher-timeout=120000",
            ],
        ),
        "content_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/webrtc.content_unittests.filter",
            ],
        ),
        "remoting_unittests": targets.legacy_test_config(
            args = [
                "--gtest_filter=Webrtc*",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "webrtc_chromium_simple_gtests",
    tests = {
        "content_browsertests": targets.legacy_test_config(
            args = [
                "--gtest_filter=WebRtc*",
            ],
        ),
        # These run a few tests on fake webcams. They need to run sequentially,
        # otherwise tests may interfere with each other.
        "content_browsertests_sequential": targets.legacy_test_config(
            test = "content_browsertests",
            args = [
                "--gtest_filter=UsingRealWebcam*",
                "--run-manual",
                "--test-launcher-jobs=1",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "webrtc_chromium_wpt_tests",
    tests = {
        "blink_wpt_tests": targets.legacy_test_config(
            test = "blink_wpt_tests",
            results_handler = "layout tests",
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            args = [
                # layout test failures are retried 3 times when '--test-list' is not
                # passed, but 0 times when '--test-list' is passed. We want to always
                # retry 3 times, so we explicitly specify it.
                "--num-retries=3",
                "-t",
                "Release",
                "external/wpt/webrtc",
                "external/wpt/webrtc-encoded-transform",
                "external/wpt/webrtc-extensions",
                "external/wpt/webrtc-priority",
                "external/wpt/webrtc-stats",
                "external/wpt/webrtc-svc",
            ],
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_64_cts_tests_gtest",
    tests = {
        "webview_64_cts_tests": targets.legacy_test_config(
            mixins = [
                "webview_cts_archive",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_bot_instrumentation_test_apk_gtest",
    tests = {
        "webview_instrumentation_test_apk": targets.legacy_test_config(
            args = [
                "--use-apk-under-test-flags-file",
            ],
            swarming = targets.swarming(
                shards = 12,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_bot_instrumentation_test_apk_mutations_gtest",
    tests = {
        "webview_instrumentation_test_apk_mutations": targets.legacy_test_config(
            test = "webview_instrumentation_test_apk",
            args = [
                "--use-apk-under-test-flags-file",
                "--webview-mutations-enabled",
            ],
            swarming = targets.swarming(
                shards = 12,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_bot_instrumentation_test_apk_no_field_trial_gtest",
    tests = {
        "webview_instrumentation_test_apk_no_field_trial": targets.legacy_test_config(
            test = "webview_instrumentation_test_apk",
            args = [
                "--disable-field-trial-config",
            ],
            swarming = targets.swarming(
                shards = 12,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_bot_unittests_gtest",
    tests = {
        "android_webview_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "webview_cts_tests_gtest",
    tests = {
        "webview_cts_tests": targets.legacy_test_config(
            mixins = [
                "webview_cts_archive",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_cts_tests_gtest_no_field_trial",
    tests = {
        "webview_cts_tests_no_field_trial": targets.legacy_test_config(
            test = "webview_cts_tests",
            mixins = [
                "webview_cts_archive",
            ],
            args = [
                "--disable-field-trial-config",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_trichrome_64_cts_field_trial_tests",
    tests = {
        "webview_trichrome_64_cts_tests": targets.legacy_test_config(
            mixins = [
                "webview_cts_archive",
            ],
            args = [
                "--store-data-dependencies-in-temp",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_trichrome_64_cts_tests",
    tests = {
        "webview_trichrome_64_cts_tests": targets.legacy_test_config(
            mixins = [
                "webview_cts_archive",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_trichrome_64_cts_tests_no_field_trial",
    tests = {
        "webview_trichrome_64_cts_tests_no_field_trial": targets.legacy_test_config(
            test = "webview_trichrome_64_cts_tests",
            mixins = [
                "webview_cts_archive",
            ],
            args = [
                "--disable-field-trial-config",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_trichrome_cts_tests",
    tests = {
        "webview_trichrome_cts_tests": targets.legacy_test_config(
            mixins = [
                "webview_cts_archive",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "webview_ui_instrumentation_tests",
    tests = {
        "webview_ui_test_app_test_apk": None,
    },
)

targets.legacy_basic_suite(
    name = "webview_ui_instrumentation_tests_no_field_trial",
    tests = {
        "webview_ui_test_app_test_apk_no_field_trial": targets.legacy_test_config(
            test = "webview_ui_test_app_test_apk",
            args = [
                "--disable-field-trial-config",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "win_specific_chromium_gtests",
    tests = {
        "chrome_elf_unittests": None,
        "courgette_unittests": None,
        "delayloads_unittests": None,
        "elevation_service_unittests": None,
        "gcp_unittests": None,
        "install_static_unittests": None,
        "installer_util_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "integrity": "high",
                },
            ),
        ),
        "notification_helper_unittests": None,
        "sbox_integration_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "integrity": "high",
                },
            ),
        ),
        "sbox_unittests": None,
        "sbox_validation_tests": None,
        "setup_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "integrity": "high",
                },
            ),
        ),
        "updater_tests": None,
        "updater_tests_system": None,
        "zucchini_unittests": None,
    },
)

targets.legacy_basic_suite(
    name = "win_specific_isolated_scripts",
    tests = {
        "mini_installer_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "integrity": "high",
                },
            ),
        ),
        "polymer_tools_python_unittests": targets.legacy_test_config(
            experiment_percentage = 0,
        ),
    },
)

targets.legacy_basic_suite(
    name = "win_specific_xr_perf_tests",
    tests = {
        "xr.webxr.static": targets.legacy_test_config(
            test = "vr_perf_tests",
            args = [
                "--benchmarks=xr.webxr.static",
                "-v",
                "--upload-results",
                "--output-format=histograms",
                "--browser=release_x64",
            ],
            merge = targets.merge(
                script = "//tools/perf/process_perf_results.py",
            ),
            # Experimental until we're sure these are stable.
            experiment_percentage = 100,
        ),
    },
)

targets.legacy_basic_suite(
    name = "wpt_tests_ios",
    tests = {
        "wpt_tests_ios": targets.legacy_test_config(
            test = "chrome_ios_wpt",
            results_handler = "layout tests",
            args = [
                "--no-wpt-internal",
            ],
            swarming = targets.swarming(
                shards = 4,
                expiration_sec = 18000,
                hard_timeout_sec = 14400,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
            experiment_percentage = 100,
        ),
    },
)

targets.legacy_basic_suite(
    name = "wpt_web_tests_android",
    tests = {
        "chrome_public_wpt": targets.legacy_test_config(
            results_handler = "layout tests",
            swarming = targets.swarming(
                shards = 15,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
        "system_webview_wpt": targets.legacy_test_config(
            results_handler = "layout tests",
            swarming = targets.swarming(
                shards = 15,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "wpt_web_tests_content_shell",
    tests = {
        "wpt_tests_suite": targets.legacy_test_config(
            test = "content_shell_wpt",
            results_handler = "layout tests",
            swarming = targets.swarming(
                shards = 15,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "wpt_web_tests_enable_leak_detection",
    tests = {
        "wpt_tests_suite": targets.legacy_test_config(
            test = "content_shell_wpt",
            results_handler = "layout tests",
            args = [
                "--enable-leak-detection",
            ],
            swarming = targets.swarming(
                shards = 15,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
            experiment_percentage = 100,
        ),
    },
)

targets.legacy_basic_suite(
    name = "wpt_web_tests_highdpi",
    tests = {
        "wpt_tests_suite_highdpi": targets.legacy_test_config(
            test = "content_shell_wpt",
            results_handler = "layout tests",
            args = [
                "--flag-specific",
                "highdpi",
            ],
            swarming = targets.swarming(
                shards = 3,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "wpt_web_tests_not_site_per_process",
    tests = {
        "wpt_tests_suite_not_site_per_process": targets.legacy_test_config(
            test = "content_shell_wpt",
            results_handler = "layout tests",
            args = [
                "--child-processes=8",
                "--flag-specific",
                "disable-site-isolation-trials",
            ],
            swarming = targets.swarming(
                shards = 10,
            ),
            merge = targets.merge(
                script = "//third_party/blink/tools/merge_web_test_results.py",
                args = [
                    "--verbose",
                ],
            ),
        ),
    },
)
