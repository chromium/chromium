# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/targets.star", "targets")

targets.tests.gtest_test(
    name = "absl_hardening_tests",
)

targets.tests.gtest_test(
    name = "accessibility_content_browsertests",
    args = [
        "--gtest_filter=*All/DumpAccessibility*/fuchsia*",
    ],
    binary = "content_browsertests",
)

targets.tests.gtest_test(
    name = "accessibility_unittests",
)

targets.tests.isolated_script_test(
    name = "android_blink_wpt_tests",
    args = [
    ],
    binary = "chrome_public_wpt",
)

targets.tests.gtest_test(
    name = "android_browsertests",
)

targets.tests.gtest_test(
    name = "android_browsertests_no_fieldtrial",
    args = [
        "--disable-field-trial-config",
    ],
    binary = "android_browsertests",
)

targets.tests.gtest_test(
    name = "android_sync_integration_tests",
)

targets.tests.gpu_telemetry_test(
    name = "android_webview_pixel_skia_gold_test",
    telemetry_test_name = "pixel",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "android_webview_junit_tests",
)

targets.tests.gtest_test(
    name = "android_webview_unittests",
)

targets.tests.gtest_test(
    name = "angle_deqp_egl_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_gles2_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_gles31_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_gles3_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_khr_gles2_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_khr_gles3_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_khr_gles31_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_gles3_rotate180_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_gles3_rotate270_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_gles3_rotate90_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_gles31_rotate180_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_gles31_rotate270_tests",
)

targets.tests.gtest_test(
    name = "angle_deqp_gles31_rotate90_tests",
)

targets.tests.gtest_test(
    name = "angle_end2end_tests",
)

targets.tests.gtest_test(
    name = "angle_unittests",
)

targets.tests.gtest_test(
    name = "angle_white_box_tests",
)

targets.tests.gtest_test(
    name = "app_shell_unittests",
)

targets.tests.gtest_test(
    name = "ash_components_unittests",
)

targets.tests.gtest_test(
    name = "ash_webui_unittests",
)

targets.tests.gtest_test(
    name = "ash_unittests",
)

targets.tests.gtest_test(
    name = "ash_pixeltests",
    mixins = [
        "skia_gold_test",
    ],
    args = [
        "--enable-pixel-output-in-tests",
    ],
)

targets.tests.gtest_test(
    name = "aura_unittests",
)

targets.tests.isolated_script_test(
    name = "base_junit_tests",
)

targets.tests.gtest_test(
    name = "base_unittests",
)

targets.tests.gtest_test(
    name = "bf_cache_android_browsertests",
    args = [
        "--disable-features=BackForwardCache",
    ],
    binary = "android_browsertests",
)

targets.tests.gtest_test(
    name = "bf_cache_content_browsertests",
    args = [
        "--disable-features=BackForwardCache",
    ],
    binary = "content_browsertests",
)

targets.tests.gtest_test(
    name = "bf_cache_browser_tests",
    args = [
        "--disable-features=BackForwardCache",
    ],
    binary = "browser_tests",
)

targets.tests.gtest_test(
    name = "blink_common_unittests",
)

targets.tests.gtest_test(
    name = "blink_fuzzer_unittests",
)

targets.tests.gtest_test(
    name = "blink_heap_unittests",
)

targets.tests.gtest_test(
    name = "blink_platform_unittests",
    mixins = [
        "skia_gold_test",
    ],
)

targets.tests.isolated_script_test(
    name = "blink_python_tests",
)

targets.tests.isolated_script_test(
    name = "blink_pytype",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gtest_test(
    name = "blink_unit_tests",
    binary = "blink_unittests",
)

targets.tests.gtest_test(
    name = "blink_unittests",
)

targets.tests.isolated_script_test(
    name = "blink_web_tests",
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
)

targets.tests.isolated_script_test(
    name = "blink_web_tests_dt_tab_target",
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
    binary = "blink_web_tests",
)

targets.tests.isolated_script_test(
    name = "blink_wpt_tests",
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
)

targets.tests.gtest_test(
    name = "boringssl_crypto_tests",
)

targets.tests.gtest_test(
    name = "boringssl_ssl_tests",
)

targets.tests.isolated_script_test(
    name = "brfetch_blink_web_tests",
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
    binary = "blink_web_tests",
)

targets.tests.isolated_script_test(
    name = "brfetch_blink_wpt_tests",
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
    binary = "blink_wpt_tests",
)

targets.tests.isolated_script_test(
    name = "brfetch_headless_shell_wpt_tests",
    mixins = [
        "has_native_resultdb_integration",
        "blink_tests_write_run_histories",
    ],
    args = [
        "--flag-specific=background-resource-fetch",
        "--skipped=always",
        "--inverted-test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/chrome.filter",
        "--test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/headless_shell.filter",
    ],
    binary = "headless_shell_wpt",
)

targets.tests.gtest_test(
    name = "browser_tests",
)

targets.tests.gtest_test(
    name = "browser_tests_functional",
    args = [
        "--test-launcher-filter-file=../../testing/buildbot/filters/webrtc_functional.browser_tests.filter",
        "--run-manual",
        "--test-launcher-jobs=1",
    ],
    binary = "browser_tests",
)

targets.tests.gtest_test(
    name = "browser_tests_network_sandbox",
    args = [
        "--enable-features=NetworkServiceSandbox",
    ],
    binary = "browser_tests",
)

targets.tests.gtest_test(
    name = "browser_tests_no_field_trial",
    args = [
        "--disable-field-trial-config",
    ],
    binary = "browser_tests",
)

targets.tests.isolated_script_test(
    name = "build_junit_tests",
)

targets.tests.gtest_test(
    name = "capture_unittests",
)

targets.tests.gtest_test(
    name = "cast_runner_browsertests",
)

targets.tests.gtest_test(
    name = "cast_runner_integration_tests",
)

targets.tests.gtest_test(
    name = "cast_runner_unittests",
)

targets.tests.junit_test(
    name = "cast_base_junit_tests",
    label = "//chromecast/base:cast_base_junit_tests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

# TODO(issues.chromium.org/1516671): Eliminate cast_* suites that are no longer
# needed.

targets.tests.gtest_test(
    name = "cast_android_cma_backend_unittests",
)

targets.tests.gtest_test(
    name = "cast_audio_backend_unittests",
)

targets.tests.gtest_test(
    name = "cast_base_unittests",
)

targets.tests.gtest_test(
    name = "cast_cast_core_unittests",
)

targets.tests.gtest_test(
    name = "cast_crash_unittests",
)

targets.tests.gtest_test(
    name = "cast_display_settings_unittests",
)

targets.tests.gtest_test(
    name = "cast_graphics_unittests",
)

targets.tests.gtest_test(
    name = "cast_media_unittests",
)

targets.tests.gtest_test(
    name = "cast_shell_browsertests",
)

targets.tests.gtest_test(
    name = "cast_shell_unittests",
)

targets.tests.junit_test(
    name = "cast_shell_junit_tests",
    label = "//chromecast/browser/android:cast_shell_junit_tests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.tests.gtest_test(
    name = "cast_unittests",
)

targets.tests.gtest_test(
    name = "cc_unittests",
)

targets.tests.script_test(
    name = "check_network_annotations",
    script = "check_network_annotations.py",
)

targets.tests.script_test(
    name = "check_static_initializers",
    script = "check_static_initializers.py",
)

targets.tests.script_test(
    name = "checkbins",
    script = "checkbins.py",
)

targets.tests.script_test(
    name = "checkdeps",
    script = "checkdeps.py",
)

targets.tests.script_test(
    name = "checkperms",
    script = "checkperms.py",
)

targets.tests.gtest_test(
    name = "chrome_all_tast_tests",
)

targets.tests.gtest_test(
    name = "chrome_app_unittests",
)

targets.tests.gtest_test(
    name = "chrome_criticalstaging_tast_tests",
)

targets.tests.gtest_test(
    name = "chrome_disabled_tast_tests",
)

targets.tests.gtest_test(
    name = "chrome_elf_unittests",
)

targets.tests.isolated_script_test(
    name = "chrome_java_test_pagecontroller_junit_tests",
)

targets.tests.gtest_test(
    name = "chrome_java_test_wpr_tests",
    mixins = [
        "skia_gold_test",
    ],
)

targets.tests.isolated_script_test(
    name = "chrome_junit_tests",
)

targets.tests.gtest_test(
    name = "chrome_ml_unittests",
)

targets.tests.isolated_script_test(
    name = "chrome_private_code_test",
)

targets.tests.gtest_test(
    name = "chrome_public_apk_profile_tests",
    binary = "chrome_public_apk_baseline_profile_generator",
)

targets.tests.gtest_test(
    name = "chrome_public_smoke_test",
)

targets.tests.gtest_test(
    name = "chrome_public_test_apk",
    mixins = [
        "skia_gold_test",
    ],
)

targets.tests.gtest_test(
    name = "chrome_public_test_vr_apk",
)

targets.tests.gtest_test(
    name = "chrome_public_unit_test_apk",
    mixins = [
        "skia_gold_test",
    ],
)

targets.tests.isolated_script_test(
    name = "chrome_public_wpt",
)

targets.tests.isolated_script_test(
    name = "chrome_sizes",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "chromedriver_py_tests",
)

targets.tests.isolated_script_test(
    name = "chromedriver_py_tests_headless_shell",
)

targets.tests.gtest_test(
    name = "chromeos_integration_tests",
)

targets.tests.gtest_test(
    name = "chromeos_js_code_coverage_browser_tests",
    binary = "browser_tests",
)

targets.tests.isolated_script_test(
    name = "chrome_wpt_tests",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/chrome.filter",
    ],
)

targets.tests.isolated_script_test(
    name = "chrome_wpt_tests_headful",
    mixins = [
        "has_native_resultdb_integration",
    ],
    binary = "chrome_wpt_tests",
)

targets.tests.isolated_script_test(
    name = "headless_shell_wpt_tests",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--test-type",
        "testharness",
        "reftest",
        "crashtest",
        "print-reftest",
        "--inverted-test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/chrome.filter",
        "--test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/headless_shell.filter",
    ],
    binary = "headless_shell_wpt",
)

targets.tests.isolated_script_test(
    name = "headless_shell_wpt_tests_include_all",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--inverted-test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/chrome.filter",
    ],
    binary = "headless_shell_wpt",
)

targets.tests.gtest_test(
    name = "openscreen_unittests",
)

targets.tests.isolated_script_test(
    name = "chromedriver_replay_unittests",
)

targets.tests.gtest_test(
    name = "chromedriver_unittests",
)

targets.tests.gtest_test(
    name = "chromeos_components_unittests",
)

targets.tests.gtest_test(
    name = "chromeos_unittests",
)

targets.tests.gtest_test(
    name = "components_browsertests",
)

targets.tests.gtest_test(
    name = "components_browsertests_network_sandbox",
    args = [
        "--enable-features=NetworkServiceSandbox",
    ],
    binary = "components_browsertests",
)

targets.tests.gtest_test(
    name = "components_browsertests_no_field_trial",
    args = [
        "--disable-field-trial-config",
    ],
    binary = "components_browsertests",
)

targets.tests.isolated_script_test(
    name = "components_junit_tests",
)

targets.tests.isolated_script_test(
    name = "components_perftests",
)

targets.tests.gtest_test(
    name = "components_unittests",
)

targets.tests.gtest_test(
    name = "compositor_unittests",
)

targets.tests.gtest_test(
    name = "content_browsertests",
)

targets.tests.gtest_test(
    name = "content_browsertests_network_sandbox",
    args = [
        "--enable-features=NetworkServiceSandbox",
    ],
    binary = "content_browsertests",
)

# These run a few tests on fake webcams. They need to run sequentially,
# otherwise tests may interfere with each other.
targets.tests.gtest_test(
    name = "content_browsertests_sequential",
    args = [
        "--gtest_filter=UsingRealWebcam*",
        "--run-manual",
        "--test-launcher-jobs=1",
    ],
    binary = "content_browsertests",
)

targets.tests.gtest_test(
    name = "content_browsertests_stress",
    args = [
        "--gtest_filter=WebRtc*MANUAL*:-UsingRealWebcam*",
        "--run-manual",
        "--ui-test-action-max-timeout=110000",
        "--test-launcher-timeout=120000",
    ],
    binary = "content_browsertests",
)

targets.tests.gtest_test(
    name = "content_browsertests_with_emulator_network",
    mixins = [
        "emulator-enable-network",
    ],
    args = [
        # These are integration tests for network change, they need to run
        # on an emulator with network enabled
        "--gtest_filter=QuicConnectionMigrationTest.*",
    ],
    binary = "content_browsertests",
)

targets.tests.isolated_script_test(
    name = "content_junit_tests",
)

targets.tests.isolated_script_test(
    name = "content_shell_crash_test",
)

targets.tests.gtest_test(
    name = "content_shell_test_apk",
)

targets.tests.gtest_test(
    name = "content_unittests",
)

targets.tests.gpu_telemetry_test(
    name = "context_lost_gl_passthrough_ganesh_tests",
    telemetry_test_name = "context_lost",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "context_lost_metal_passthrough_ganesh_tests",
    telemetry_test_name = "context_lost",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "context_lost_metal_passthrough_graphite_tests",
    telemetry_test_name = "context_lost",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "context_lost_passthrough_tests",
    telemetry_test_name = "context_lost",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "context_lost_passthrough_graphite_tests",
    telemetry_test_name = "context_lost",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "context_lost_validating_tests",
    telemetry_test_name = "context_lost",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gtest_test(
    name = "crashpad_tests",
)

targets.tests.gtest_test(
    name = "cronet_sample_test_apk",
)

targets.tests.isolated_script_test(
    name = "cronet_sizes",
)

targets.tests.gtest_test(
    name = "cronet_smoketests_apk",
)

targets.tests.gtest_test(
    name = "cronet_smoketests_missing_native_library_instrumentation_apk",
)

targets.tests.gtest_test(
    name = "cronet_smoketests_platform_only_instrumentation_apk",
)

targets.tests.gtest_test(
    name = "cronet_test_instrumentation_apk",
)

targets.tests.gtest_test(
    name = "cronet_tests",
)

targets.tests.gtest_test(
    name = "cronet_tests_android",
)

targets.tests.gtest_test(
    name = "cronet_unittests",
)

targets.tests.gtest_test(
    name = "cronet_unittests_android",
)

targets.tests.gtest_test(
    name = "crypto_unittests",
)

targets.tests.isolated_script_test(
    name = "dawn_perf_tests",
    args = [
        # Tell the tests to only run one step for faster iteration.
        "--override-steps=1",
        "--gtest-benchmark-name=dawn_perf_tests",
        "-v",
    ],
)

targets.tests.gtest_test(
    name = "dbus_unittests",
)

targets.tests.gtest_test(
    name = "delayloads_unittests",
)

targets.tests.isolated_script_test(
    name = "device_junit_tests",
)

targets.tests.gtest_test(
    name = "device_unittests",
)

targets.tests.gtest_test(
    name = "devtools_browser_tests",
    args = [
        "--gtest_filter=*DevTools*",
    ],
    binary = "browser_tests",
)

targets.tests.gtest_test(
    name = "disk_usage_tast_test",
)

targets.tests.gtest_test(
    name = "display_unittests",
)

targets.tests.gtest_test(
    name = "elevation_service_unittests",
)

targets.tests.gtest_test(
    name = "enterprise_companion_integration_tests",
)

targets.tests.gtest_test(
    name = "enterprise_companion_tests",
)

targets.tests.gtest_test(
    name = "env_chromium_unittests",
)

targets.tests.gtest_test(
    name = "events_unittests",
)

targets.tests.gtest_test(
    name = "exo_unittests",
)

targets.tests.gpu_telemetry_test(
    name = "expected_color_pixel_gl_passthrough_ganesh_test",
    telemetry_test_name = "expected_color",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "expected_color_pixel_metal_passthrough_ganesh_test",
    telemetry_test_name = "expected_color",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "expected_color_pixel_metal_passthrough_graphite_test",
    telemetry_test_name = "expected_color",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "expected_color_pixel_passthrough_graphite_test",
    telemetry_test_name = "expected_color",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "expected_color_pixel_passthrough_test",
    telemetry_test_name = "expected_color",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "expected_color_pixel_validating_test",
    telemetry_test_name = "expected_color",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gtest_test(
    name = "extensions_browsertests",
)

targets.tests.gtest_test(
    name = "extensions_browsertests_network_sandbox",
    args = [
        "--enable-features=NetworkServiceSandbox",
    ],
    binary = "extensions_browsertests",
)

targets.tests.gtest_test(
    name = "extensions_unittests",
)

targets.tests.gtest_test(
    name = "fake_libva_driver_unittest",
    args = [
        "--env-var",
        "LIBVA_DRIVERS_PATH",
        "./",
        "--env-var",
        "LIBVA_DRIVER_NAME",
        "libfake",
    ],
)

targets.tests.gtest_test(
    name = "video_decode_accelerator_tests_fake_vaapi",
    args = [
        "--env-var",
        "LIBVA_DRIVERS_PATH",
        "./",
        "--env-var",
        "LIBVA_DRIVER_NAME",
        "libfake",
        "../../media/test/data/test-25fps.vp9",
        "../../media/test/data/test-25fps.vp9.json",
    ],
    binary = "video_decode_accelerator_tests",
)

targets.tests.gtest_test(
    name = "video_decode_accelerator_tests_v4l2_vp8",
    args = [
        "--as-root",
        "--validator_type=none",
        "../../media/test/data/test-25fps.vp8",
        "../../media/test/data/test-25fps.vp8.json",
    ],
    binary = "video_decode_accelerator_tests",
)

targets.tests.gtest_test(
    name = "video_decode_accelerator_tests_v4l2_vp9",
    args = [
        "--as-root",
        "--validator_type=none",
        "../../media/test/data/test-25fps.vp9",
        "../../media/test/data/test-25fps.vp9.json",
    ],
    binary = "video_decode_accelerator_tests",
)

targets.tests.gtest_test(
    name = "filesystem_service_unittests",
)

targets.tests.isolated_script_test(
    name = "flatbuffers_unittests",
)

targets.tests.isolated_script_test(
    name = "fuchsia_pytype",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "fuchsia_sizes",
    args = [
        "--sizes-path",
        "tools/fuchsia/size_tests/fyi_sizes_smoketest.json",
    ],
)

targets.tests.gtest_test(
    name = "gcm_unit_tests",
)

targets.tests.gtest_test(
    name = "gcp_unittests",
)

targets.tests.gtest_test(
    name = "gfx_unittests",
)

targets.tests.gtest_test(
    name = "gin_unittests",
)

targets.tests.gtest_test(
    name = "gl_tests_passthrough",
    args = [
        "--use-cmd-decoder=passthrough",
    ],
    binary = "gl_tests",
)

targets.tests.gtest_test(
    name = "gl_tests_validating",
    args = [
        "--use-cmd-decoder=validating",
    ],
    binary = "gl_tests",
)

targets.tests.gtest_test(
    name = "gl_unittests",
)

targets.tests.gtest_test(
    name = "gl_unittests_ozone",
)

targets.tests.gtest_test(
    name = "gpu_memory_buffer_impl_tests",
    binary = "gpu_unittests",
)

targets.tests.isolated_script_test(
    name = "gold_common_pytype",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gtest_test(
    name = "google_apis_unittests",
)

targets.tests.gtest_test(
    name = "dawn_end2end_implicit_device_sync_tests",
    mixins = [
        "dawn_end2end_gpu_test",
    ],
    args = [
        "--enable-implicit-device-sync",
    ],
    binary = "dawn_end2end_tests",
)

targets.tests.gtest_test(
    name = "dawn_end2end_no_dxc_tests",
    mixins = [
        "dawn_end2end_gpu_test",
    ],
    args = [
        "--disable-toggles=use_dxc",
    ],
    binary = "dawn_end2end_tests",
)

targets.tests.gtest_test(
    name = "dawn_end2end_no_dxc_validation_layers_tests",
    mixins = [
        "dawn_end2end_gpu_test",
    ],
    args = [
        "--disable-toggles=use_dxc",
        "--enable-backend-validation",
    ],
    binary = "dawn_end2end_tests",
)

targets.tests.gtest_test(
    name = "dawn_end2end_skip_validation_tests",
    mixins = [
        "dawn_end2end_gpu_test",
    ],
    args = [
        "--enable-toggles=skip_validation",
    ],
    binary = "dawn_end2end_tests",
)

targets.tests.gtest_test(
    name = "dawn_end2end_tests",
    mixins = [
        "dawn_end2end_gpu_test",
    ],
)

targets.tests.gtest_test(
    name = "dawn_end2end_validation_layers_tests",
    mixins = [
        "dawn_end2end_gpu_test",
    ],
    args = [
        "--enable-backend-validation",
    ],
    binary = "dawn_end2end_tests",
)

targets.tests.gtest_test(
    name = "dawn_end2end_wire_tests",
    mixins = [
        "dawn_end2end_gpu_test",
    ],
    args = [
        "--use-wire",
    ],
    binary = "dawn_end2end_tests",
)

targets.tests.gtest_test(
    name = "dawn_end2end_use_tint_ir_tests",
    mixins = [
        "dawn_end2end_gpu_test",
    ],
    args = [
        "--enable-toggles=use_tint_ir",
    ],
    binary = "dawn_end2end_tests",
)

targets.tests.gtest_test(
    name = "elevated_tracing_service_unittests",
)

targets.tests.gtest_test(
    name = "fuzzing_unittests",
)

targets.tests.gpu_telemetry_test(
    name = "gpu_process_launch_tests",
    telemetry_test_name = "gpu_process",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "gpu_pytype",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gtest_test(
    name = "gpu_unittests",
)

targets.tests.isolated_script_test(
    # graphite_enabled_blink_web_tests provides coverage for
    # running Layout Tests with Skia Graphite.
    name = "graphite_enabled_blink_web_tests",
    mixins = [
        "has_native_resultdb_integration",
        "blink_tests_write_run_histories",
    ],
    args = [
        "--flag-specific=enable-skia-graphite",
        "--skipped=always",
        # Since there are random timeouts, we have to increase the timeout
        # threshold for now.
        # TODO(crbug.com/41490824): Remove this once we resolve the timeouts.
        "--timeout-ms=20000",
        # layout test failures are retried 3 times when '--test-list' is not
        # passed, but 0 times when '--test-list' is passed. We want to always
        # retry 3 times, so we explicitly specify it.
        "--num-retries=3",
    ],
    binary = "blink_web_tests",
)

targets.tests.isolated_script_test(
    # graphite_enabled_blink_wpt_tests provides coverage for
    # running Layout Tests with Skia Graphite.
    name = "graphite_enabled_blink_wpt_tests",
    mixins = [
        "has_native_resultdb_integration",
        "blink_tests_write_run_histories",
    ],
    args = [
        "--flag-specific=enable-skia-graphite",
        "--skipped=always",
        # Since there are random timeouts, we have to increase the timeout
        # threshold for now.
        # TODO(crbug.com/41490824): Remove this once we resolve the timeouts.
        "--timeout-ms=20000",
        # layout test failures are retried 3 times when '--test-list' is not
        # passed, but 0 times when '--test-list' is passed. We want to always
        # retry 3 times, so we explicitly specify it.
        "--num-retries=3",
    ],
    binary = "blink_wpt_tests",
)

targets.tests.isolated_script_test(
    # graphite_enabled_headless_shell_wpt_tests provides coverage for
    # running web platform tests with Skia Graphite.
    name = "graphite_enabled_headless_shell_wpt_tests",
    mixins = [
        "has_native_resultdb_integration",
        "blink_tests_write_run_histories",
    ],
    args = [
        "--flag-specific=enable-skia-graphite",
        "--skipped=always",
        # Since there are random timeouts, we have to increase the timeout
        # threshold for now.
        # TODO(crbug.com/41490824): Remove this once we resolve the timeouts.
        "--timeout-multiplier=2",
        "--inverted-test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/chrome.filter",
        "--test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/headless_shell.filter",
    ],
    binary = "headless_shell_wpt",
)

targets.tests.isolated_script_test(
    name = "grit_python_unittests",
)

targets.tests.gtest_test(
    name = "gwp_asan_unittests",
)

targets.tests.gpu_telemetry_test(
    name = "hardware_accelerated_feature_tests",
    telemetry_test_name = "hardware_accelerated_feature",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gtest_test(
    name = "headless_browsertests",
)

targets.tests.gtest_test(
    name = "headless_unittests",
)

targets.tests.isolated_script_test(
    name = "high_dpi_blink_web_tests",
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
    binary = "blink_web_tests",
)

targets.tests.isolated_script_test(
    name = "high_dpi_blink_wpt_tests",
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
    binary = "blink_wpt_tests",
)

targets.tests.isolated_script_test(
    name = "high_dpi_headless_shell_wpt_tests",
    mixins = [
        "has_native_resultdb_integration",
        "blink_tests_write_run_histories",
    ],
    args = [
        "--flag-specific=highdpi",
        "--skipped=always",
        "--inverted-test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/chrome.filter",
        "--test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/headless_shell.filter",
    ],
    binary = "headless_shell_wpt",
)

targets.tests.gpu_telemetry_test(
    name = "info_collection_tests",
    telemetry_test_name = "info_collection",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gtest_test(
    name = "install_static_unittests",
)

targets.tests.gtest_test(
    name = "installer_util_unittests",
)

targets.tests.gtest_test(
    name = "interactive_ui_tests",
)

targets.tests.gtest_test(
    name = "interactive_ui_tests_network_sandbox",
    args = [
        "--enable-features=NetworkServiceSandbox",
    ],
    binary = "interactive_ui_tests",
)

targets.tests.gtest_test(
    name = "interactive_ui_tests_no_field_trial",
    args = [
        "--disable-field-trial-config",
    ],
    binary = "interactive_ui_tests",
)

targets.tests.isolated_script_test(
    name = "ios_chrome_unittests",
)

targets.tests.isolated_script_test(
    name = "ios_chrome_bookmarks_eg2tests_module",
)

targets.tests.isolated_script_test(
    name = "ios_chrome_integration_eg2tests_module",
)

targets.tests.isolated_script_test(
    name = "ios_chrome_settings_eg2tests_module",
)

targets.tests.isolated_script_test(
    name = "ios_chrome_signin_eg2tests_module",
)

targets.tests.isolated_script_test(
    name = "ios_chrome_smoke_eg2tests_module",
)

targets.tests.isolated_script_test(
    name = "ios_chrome_ui_eg2tests_module",
)

targets.tests.isolated_script_test(
    name = "ios_chrome_web_eg2tests_module",
)

targets.tests.isolated_script_test(
    name = "ios_crash_xcuitests_module",
)

targets.tests.isolated_script_test(
    name = "ios_components_unittests",
)

targets.tests.isolated_script_test(
    name = "ios_net_unittests",
)

targets.tests.isolated_script_test(
    name = "ios_testing_unittests",
)

targets.tests.isolated_script_test(
    name = "ios_web_inttests",
)

targets.tests.isolated_script_test(
    name = "ios_web_shell_eg2tests_module",
)

targets.tests.isolated_script_test(
    name = "ios_web_unittests",
)

targets.tests.isolated_script_test(
    name = "ios_web_view_inttests",
)

targets.tests.isolated_script_test(
    name = "ios_web_view_unittests",
)

targets.tests.gtest_test(
    name = "ipc_tests",
)

targets.tests.gtest_test(
    name = "js_code_coverage_browser_tests",
    binary = "browser_tests",
)

targets.tests.isolated_script_test(
    name = "junit_unit_tests",
)

targets.tests.junit_test(
    name = "keyboard_accessory_junit_tests",
    label = "//chrome/android/features/keyboard_accessory:keyboard_accessory_junit_tests",
)

targets.tests.gtest_test(
    name = "keyboard_unittests",
)

targets.tests.gtest_test(
    name = "latency_unittests",
)

targets.tests.gtest_test(
    name = "leveldb_unittests",
)

targets.tests.gtest_test(
    name = "libcups_unittests",
)

targets.tests.gtest_test(
    name = "libjingle_xmpp_unittests",
)

targets.tests.gtest_test(
    name = "liburlpattern_unittests",
)

targets.tests.isolated_script_test(
    name = "mac_signing_tests",
)

targets.tests.isolated_script_test(
    name = "media_base_junit_tests",
)

targets.tests.gtest_test(
    name = "media_foundation_browser_tests",
    args = [
        "--gtest_filter=MediaFoundationEncryptedMediaTest*",
    ],
    binary = "browser_tests",
)

targets.tests.gtest_test(
    name = "media_unittests",
)

targets.tests.gtest_test(
    name = "media_unittests_skia_graphite_dawn",
    args = [
        "--test-launcher-bot-mode",
        "--enable-features=SkiaGraphite",
        "--skia-graphite-backend=dawn",
        "--use-gpu-in-tests",
    ],
    binary = "media_unittests",
)

targets.tests.gtest_test(
    name = "media_unittests_skia_graphite_metal",
    args = [
        "--test-launcher-bot-mode",
        "--enable-features=SkiaGraphite",
        "--skia-graphite-backend=metal",
        "--use-gpu-in-tests",
    ],
    binary = "media_unittests",
)

targets.tests.isolated_script_test(
    name = "memory.leak_detection",
    args = [
        "--pageset-repeat=1",
        "--test-shard-map-filename=linux_leak_detection_shard_map.json",
        "--upload-results",
        "--output-format=histograms",
        "--browser=release",
        "--xvfb",
    ],
    binary = "performance_test_suite",
)

targets.tests.script_test(
    name = "metrics_python_tests",
    script = "metrics_python_tests.py",
)

targets.tests.gtest_test(
    name = "message_center_unittests",
)

targets.tests.gtest_test(
    name = "midi_unittests",
)

targets.tests.isolated_script_test(
    name = "mini_installer_tests",
)

targets.tests.gtest_test(
    name = "minidump_uploader_test",
)

targets.tests.isolated_script_test(
    name = "model_validation_tests",
)

targets.tests.isolated_script_test(
    name = "model_validation_tests_light",
)

targets.tests.isolated_script_test(
    name = "module_installer_junit_tests",
)

targets.tests.gtest_test(
    name = "monochrome_public_smoke_test",
)

targets.tests.gtest_test(
    name = "monochrome_public_bundle_smoke_test",
)

targets.tests.isolated_script_test(
    name = "mojo_python_unittests",
)

targets.tests.gtest_test(
    name = "mojo_rust_integration_unittests",
)

targets.tests.gtest_test(
    name = "mojo_rust_unittests",
)

targets.tests.gtest_test(
    name = "mojo_test_apk",
)

targets.tests.gtest_test(
    name = "mojo_unittests",
)

targets.tests.isolated_script_test(
    name = "monochrome_public_apk_checker",
)

targets.tests.gtest_test(
    name = "monochrome_public_test_ar_apk",
)

targets.tests.gtest_test(
    name = "multiscreen_interactive_ui_tests",
    binary = "interactive_ui_tests",
)

targets.tests.gtest_test(
    name = "nacl_loader_unittests",
)

targets.tests.isolated_script_test(
    name = "build_rust_tests",
)

targets.tests.gtest_test(
    name = "native_theme_unittests",
)

targets.tests.isolated_script_test(
    name = "net_junit_tests",
)

targets.tests.gtest_test(
    name = "net_unittests",
)

targets.tests.gtest_test(
    name = "network_service_web_request_proxy_browser_tests",
    args = [
        "--enable-features=ForceWebRequestProxyForTest",
    ],
    binary = "browser_tests",
)

targets.tests.gpu_telemetry_test(
    name = "noop_sleep_tests",
    telemetry_test_name = "noop_sleep",
)

targets.tests.isolated_script_test(
    name = "not_site_per_process_blink_web_tests",
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
    binary = "blink_web_tests",
)

targets.tests.isolated_script_test(
    name = "not_site_per_process_blink_wpt_tests",
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
    binary = "blink_wpt_tests",
)

targets.tests.isolated_script_test(
    name = "not_site_per_process_headless_shell_wpt_tests",
    mixins = [
        "has_native_resultdb_integration",
        "blink_tests_write_run_histories",
    ],
    args = [
        "--flag-specific=disable-site-isolation-trials",
        "--inverted-test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/chrome.filter",
        "--test-launcher-filter-file=../../third_party/blink/web_tests/TestLists/headless_shell.filter",
    ],
    binary = "headless_shell_wpt",
)

targets.tests.gtest_test(
    name = "notification_helper_unittests",
)

targets.tests.isolated_script_test(
    name = "ondevice_quality_tests",
)

targets.tests.isolated_script_test(
    name = "ondevice_stability_tests",
)

targets.tests.isolated_script_test(
    name = "ondevice_stability_tests_light",
)

targets.tests.isolated_script_test(
    name = "chrome_ai_wpt_tests",
)

targets.tests.gtest_test(
    name = "oobe_only_browser_tests",
    args = [
        "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.msan.browser_tests.oobe_positive.filter",
    ],
    binary = "browser_tests",
)

targets.tests.gtest_test(
    name = "optimization_guide_browser_tests",
    args = [
        "--gtest_filter=*OptimizationGuide*:*PageContentAnnotations*",
    ],
    binary = "browser_tests",
)

targets.tests.gtest_test(
    name = "optimization_guide_components_unittests",
    args = [
        "--gtest_filter=*OptimizationGuide*:*PageEntities*:*EntityAnnotator*",
    ],
    binary = "components_unittests",
)

targets.tests.gtest_test(
    name = "optimization_guide_gpu_unittests",
)

targets.tests.gtest_test(
    name = "optimization_guide_unittests",
)

targets.tests.gtest_test(
    name = "ozone_gl_unittests",
)

targets.tests.gtest_test(
    name = "ozone_unittests",
)

targets.tests.gtest_test(
    name = "ozone_x11_unittests",
)

targets.tests.isolated_script_test(
    name = "paint_preview_junit_tests",
)

targets.tests.isolated_script_test(
    name = "passthrough_command_buffer_perftests",
    binary = "command_buffer_perftests",
)

targets.tests.isolated_script_test(
    name = "password_check_junit_tests",
)

targets.tests.isolated_script_test(
    name = "password_manager_junit_tests",
)

targets.tests.gtest_test(
    name = "pdf_unittests",
)

targets.tests.gtest_test(
    name = "perfetto_unittests",
)

targets.tests.isolated_script_test(
    name = "performance_test_suite",
)

targets.tests.gtest_test(
    name = "pixel_browser_tests",
    mixins = [
        "skia_gold_test",
    ],
    args = [
        "--browser-ui-tests-verify-pixels",
        "--enable-pixel-output-in-tests",
        "--test-launcher-filter-file=../../testing/buildbot/filters/pixel_tests.filter",
        "--test-launcher-jobs=1",
    ],
    binary = "browser_tests",
)

targets.tests.gtest_test(
    name = "pixel_experimental_browser_tests",
    mixins = [
        "skia_gold_test",
    ],
    args = [
        "--browser-ui-tests-verify-pixels",
        "--enable-pixel-output-in-tests",
        "--test-launcher-filter-file=../../testing/buildbot/filters/linux-chromeos.browser_tests.pixel_tests.filter",
    ],
    binary = "browser_tests",
)

targets.tests.gtest_test(
    name = "pixel_interactive_ui_tests",
    mixins = [
        "skia_gold_test",
    ],
    args = [
        "--browser-ui-tests-verify-pixels",
        "--enable-pixel-output-in-tests",
        "--test-launcher-filter-file=../../testing/buildbot/filters/pixel_tests.filter",
    ],
    binary = "interactive_ui_tests",
)

targets.tests.gpu_telemetry_test(
    name = "pixel_skia_gold_gl_passthrough_ganesh_test",
    telemetry_test_name = "pixel",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "pixel_skia_gold_metal_passthrough_ganesh_test",
    telemetry_test_name = "pixel",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "pixel_skia_gold_metal_passthrough_graphite_test",
    telemetry_test_name = "pixel",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "pixel_skia_gold_passthrough_graphite_test",
    telemetry_test_name = "pixel",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "pixel_skia_gold_passthrough_test",
    telemetry_test_name = "pixel",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "pixel_skia_gold_validating_test",
    telemetry_test_name = "pixel",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "polymer_tools_python_unittests",
)

targets.tests.gtest_test(
    name = "power_sampler_unittests",
)

targets.tests.isolated_script_test(
    name = "ppapi_blink_web_tests",
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
    binary = "blink_web_tests",
)

targets.tests.gtest_test(
    name = "ppapi_unittests",
)

targets.tests.gtest_test(
    name = "printing_unittests",
)

targets.tests.isolated_script_test(
    name = "private_code_failure_test",
)

targets.tests.gtest_test(
    name = "profile_provider_unittest",
)

targets.tests.gtest_test(
    name = "pthreadpool_unittests",
)

targets.tests.gtest_test(
    name = "remoting_unittests",
)

targets.tests.isolated_script_test(
    name = "resource_sizes_cronet_sample_apk",
)

targets.tests.gtest_test(
    name = "rust_gtest_interop_unittests",
)

targets.tests.gtest_test(
    name = "sandbox_linux_unittests",
)

targets.tests.gtest_test(
    name = "sandbox_unittests",
)

targets.tests.gtest_test(
    name = "sbox_integration_tests",
)

targets.tests.gtest_test(
    name = "sbox_unittests",
)

targets.tests.gtest_test(
    name = "sbox_validation_tests",
)

targets.tests.gpu_telemetry_test(
    name = "screenshot_sync_gl_passthrough_ganesh_tests",
    telemetry_test_name = "screenshot_sync",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "screenshot_sync_metal_passthrough_ganesh_tests",
    telemetry_test_name = "screenshot_sync",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "screenshot_sync_metal_passthrough_graphite_tests",
    telemetry_test_name = "screenshot_sync",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "screenshot_sync_passthrough_graphite_tests",
    telemetry_test_name = "screenshot_sync",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "screenshot_sync_passthrough_tests",
    telemetry_test_name = "screenshot_sync",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "screenshot_sync_validating_tests",
    telemetry_test_name = "screenshot_sync",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "services_junit_tests",
)

targets.tests.gtest_test(
    name = "services_unittests",
)

targets.tests.gtest_test(
    name = "services_webnn_unittests",
    args = [
        "--gtest_filter=WebNN*",
    ],
    binary = "services_unittests",
)

targets.tests.gtest_test(
    name = "setup_unittests",
)

targets.tests.gtest_test(
    name = "shell_encryption_unittests",
)

targets.tests.gtest_test(
    name = "shell_dialogs_unittests",
)

targets.tests.gtest_test(
    name = "site_per_process_android_browsertests",
    args = [
        "--site-per-process",
    ],
    binary = "android_browsertests",
)

targets.tests.gtest_test(
    name = "site_per_process_chrome_public_test_apk",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
    args = [
        "--site-per-process",
    ],
    binary = "chrome_public_test_apk",
)

targets.tests.gtest_test(
    name = "site_per_process_chrome_public_unit_test_apk",
    mixins = [
        "skia_gold_test",
    ],
    args = [
        "--site-per-process",
    ],
    binary = "chrome_public_unit_test_apk",
)

targets.tests.gtest_test(
    name = "site_per_process_components_browsertests",
    args = [
        "--site-per-process",
    ],
    binary = "components_browsertests",
)

targets.tests.gtest_test(
    name = "site_per_process_components_unittests",
    args = [
        "--site-per-process",
    ],
    binary = "components_unittests",
)

targets.tests.gtest_test(
    name = "site_per_process_content_browsertests",
    args = [
        "--site-per-process",
        "--test-launcher-filter-file=../../testing/buildbot/filters/site_isolation_android.content_browsertests.filter",
    ],
    binary = "content_browsertests",
)

targets.tests.gtest_test(
    name = "site_per_process_content_shell_test_apk",
    args = [
        "--site-per-process",
    ],
    binary = "content_shell_test_apk",
)

targets.tests.gtest_test(
    name = "site_per_process_content_unittests",
    args = [
        "--site-per-process",
    ],
    binary = "content_unittests",
)

targets.tests.gtest_test(
    name = "site_per_process_unit_tests",
    args = [
        "--site-per-process",
    ],
    binary = "unit_tests",
)

targets.tests.gtest_test(
    name = "skia_unittests",
)

targets.tests.gtest_test(
    name = "snapshot_unittests",
)

targets.tests.gtest_test(
    name = "sql_unittests",
)

targets.tests.gtest_test(
    name = "storage_unittests",
)

targets.tests.gtest_test(
    name = "sync_integration_tests",
)

targets.tests.gtest_test(
    name = "sync_integration_tests_network_sandbox",
    args = [
        "--enable-features=NetworkServiceSandbox",
    ],
    binary = "sync_integration_tests",
)

targets.tests.gtest_test(
    name = "sync_integration_tests_no_field_trial",
    args = [
        "--disable-field-trial-config",
    ],
    binary = "sync_integration_tests",
)

targets.tests.gtest_test(
    name = "system_webview_shell_layout_test_apk",
)

targets.tests.isolated_script_test(
    name = "system_webview_wpt",
)

targets.tests.gtest_test(
    name = "tab_capture_end2end_tests",
    binary = "browser_tests",
)

targets.tests.gtest_test(
    name = "tablet_sensitive_chrome_public_test_apk",
    mixins = [
        "skia_gold_test",
    ],
    args = [
        "--annotation=Restriction=Tablet,ImportantFormFactors=Tablet",
    ],
    binary = "chrome_public_test_apk",
)

targets.tests.isolated_script_test(
    name = "telemetry_chromium_minidump_unittests",
    args = [
        "BrowserMinidumpTest",
        "--browser=android-chromium",
        "-v",
        "--passthrough",
        "--retry-limit=2",
    ],
    binary = "telemetry_perf_unittests_android_chrome",
)

targets.tests.isolated_script_test(
    name = "telemetry_desktop_minidump_unittests",
    args = [
        "BrowserMinidumpTest",
        "-v",
        "--passthrough",
        "--retry-limit=2",
    ],
    binary = "telemetry_perf_unittests",
)

targets.tests.isolated_script_test(
    name = "telemetry_gpu_unittests",
)

targets.tests.isolated_script_test(
    name = "telemetry_monochrome_minidump_unittests",
    args = [
        "BrowserMinidumpTest",
        "--browser=android-chromium-monochrome",
        "-v",
        "--passthrough",
        "--retry-limit=2",
    ],
    binary = "telemetry_perf_unittests_android_monochrome",
)

targets.tests.isolated_script_test(
    name = "telemetry_perf_unittests",
)

targets.tests.isolated_script_test(
    name = "telemetry_perf_unittests_android_chrome",
)

targets.tests.isolated_script_test(
    name = "telemetry_unittests",
)

targets.tests.gtest_test(
    name = "test_cpp_including_rust_unittests",
)

targets.tests.isolated_script_test(
    name = "test_env_py_unittests",
)

targets.tests.gtest_test(
    name = "jni_zero_sample_apk_test",
)

targets.tests.gtest_test(
    name = "test_serde_json_lenient",
)

targets.tests.script_test(
    name = "test_traffic_annotation_auditor",
    script = "test_traffic_annotation_auditor.py",
    precommit_args = [
        "--no-update-sheet",
    ],
)

targets.tests.isolated_script_test(
    name = "testing_pytype",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "touch_to_fill_junit_tests",
)

targets.tests.gpu_telemetry_test(
    name = "trace_test",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gtest_test(
    name = "trichrome_chrome_bundle_smoke_test",
)

targets.tests.gtest_test(
    name = "ui_android_unittests",
)

targets.tests.gtest_test(
    name = "ui_base_unittests",
)

targets.tests.gtest_test(
    name = "ui_chromeos_unittests",
)

targets.tests.isolated_script_test(
    name = "ui_junit_tests",
)

targets.tests.gtest_test(
    name = "ui_touch_selection_unittests",
)

targets.tests.gtest_test(
    name = "unit_tests",
)

targets.tests.gtest_test(
    name = "updater_tests",
)

targets.tests.gtest_test(
    name = "updater_tests_system",
)

targets.tests.gtest_test(
    name = "updater_tests_win_uac",
)

targets.tests.gtest_test(
    name = "ui_unittests",
)

targets.tests.isolated_script_test(
    name = "upload_trace_processor",
)

targets.tests.gtest_test(
    name = "url_unittests",
)

targets.tests.gtest_test(
    name = "usage_time_limit_unittests",
)

targets.tests.gtest_test(
    name = "vaapi_unittest",
)

targets.tests.isolated_script_test(
    name = "variations_android_smoke_tests",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--target-platform=android",
    ],
    binary = "variations_desktop_smoke_tests",
)

targets.tests.isolated_script_test(
    name = "variations_desktop_smoke_tests",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "variations_smoke_tests",
    mixins = [
        "skia_gold_test",
    ],
)

targets.tests.isolated_script_test(
    name = "variations_webview_smoke_tests",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--target-platform=webview",
    ],
    binary = "variations_desktop_smoke_tests",
)

targets.tests.gtest_test(
    name = "views_examples_unittests",
)

targets.tests.isolated_script_test(
    name = "views_perftests",
)

targets.tests.gtest_test(
    name = "views_unittests",
)

targets.tests.gtest_test(
    name = "viz_unittests",
)

targets.tests.gtest_test(
    name = "vr_android_unittests",
)

targets.tests.gtest_test(
    name = "vr_common_unittests",
)

targets.tests.gpu_telemetry_test(
    name = "vulkan_pixel_skia_gold_test",
    telemetry_test_name = "pixel",
    mixins = [
        "skia_gold_test",
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "vulkan_swiftshader_blink_web_tests",
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
    binary = "blink_web_tests",
)

targets.tests.gtest_test(
    name = "vulkan_swiftshader_content_browsertests",
    binary = "content_browsertests",
)

targets.tests.gtest_test(
    name = "vulkan_tests",
)

targets.tests.gtest_test(
    name = "wayland_client_perftests",
)

targets.tests.gtest_test(
    name = "wayland_client_tests",
)

targets.tests.gtest_test(
    name = "web_engine_browsertests",
)

targets.tests.gtest_test(
    name = "web_engine_integration_tests",
)

targets.tests.gtest_test(
    name = "web_engine_unittests",
)

targets.tests.isolated_script_test(
    name = "webapk_client_junit_tests",
)

targets.tests.isolated_script_test(
    name = "webapk_shell_apk_h2o_junit_tests",
)

targets.tests.isolated_script_test(
    name = "webapk_shell_apk_junit_tests",
)

targets.tests.gpu_telemetry_test(
    name = "webcodecs_gl_passthrough_ganesh_tests",
    telemetry_test_name = "webcodecs",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=gl --disable-features=SkiaGraphite",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webcodecs_metal_passthrough_ganesh_tests",
    telemetry_test_name = "webcodecs",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --disable-features=SkiaGraphite",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webcodecs_metal_passthrough_graphite_tests",
    telemetry_test_name = "webcodecs",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --enable-features=SkiaGraphite",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webcodecs_graphite_tests",
    telemetry_test_name = "webcodecs",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webcodecs_tests",
    telemetry_test_name = "webcodecs",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "webdriver_wpt_tests",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--test-type=wdspec",
    ],
    binary = "chrome_wpt_tests",
)

targets.tests.gpu_telemetry_test(
    name = "webgl2_conformance_d3d11_passthrough_tests",
    telemetry_test_name = "webgl2_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl2_conformance_gl_passthrough_ganesh_tests",
    telemetry_test_name = "webgl2_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--webgl-conformance-version=2.0.1",
        targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
        # On dual-GPU devices we want the high-performance GPU to be active
        "--extra-browser-args=--use-gl=angle --use-angle=gl --use-cmd-decoder=passthrough --force_high_performance_gpu --disable-features=SkiaGraphite",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl2_conformance_gl_passthrough_tests",
    telemetry_test_name = "webgl2_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--webgl-conformance-version=2.0.1",
        targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
        # On dual-GPU devices we want the high-performance GPU to be active
        "--extra-browser-args=--use-gl=angle --use-angle=gl --use-cmd-decoder=passthrough --force_high_performance_gpu",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl2_conformance_gles_passthrough_tests",
    telemetry_test_name = "webgl2_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl2_conformance_metal_passthrough_tests",
    telemetry_test_name = "webgl2_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl2_conformance_metal_passthrough_graphite_tests",
    telemetry_test_name = "webgl2_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--webgl-conformance-version=2.0.1",
        targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
        # On dual-GPU devices we want the high-performance GPU to be active
        "--extra-browser-args=--use-gl=angle --use-angle=metal --use-cmd-decoder=passthrough --enable-features=EGLDualGPURendering,ForceHighPerformanceGPUForWebGL,SkiaGraphite",
        "--enable-metal-debug-layers",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl2_conformance_validating_tests",
    telemetry_test_name = "webgl2_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_d3d11_passthrough_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_d3d9_passthrough_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_gl_passthrough_ganesh_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        # On dual-GPU devices we want the high-performance GPU to be active
        "--extra-browser-args=--use-gl=angle --use-angle=gl --use-cmd-decoder=passthrough --force_high_performance_gpu --disable-features=SkiaGraphite",
        targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_gl_passthrough_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_gles_passthrough_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_gles_passthrough_graphite_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_metal_passthrough_ganesh_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        # On dual-GPU devices we want the high-performance GPU to be active
        "--extra-browser-args=--use-gl=angle --use-angle=metal --use-cmd-decoder=passthrough --enable-features=EGLDualGPURendering,ForceHighPerformanceGPUForWebGL --disable-features=SkiaGraphite",
        targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
        "--enable-metal-debug-layers",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_metal_passthrough_graphite_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        # On dual-GPU devices we want the high-performance GPU to be active
        "--extra-browser-args=--use-gl=angle --use-angle=metal --use-cmd-decoder=passthrough --enable-features=EGLDualGPURendering,ForceHighPerformanceGPUForWebGL,SkiaGraphite",
        targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
        "--enable-metal-debug-layers",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_metal_passthrough_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_swangle_passthrough_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_validating_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgl_conformance_vulkan_passthrough_tests",
    telemetry_test_name = "webgl1_conformance",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "webgpu_blink_web_tests",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "webgpu_blink_web_tests_with_backend_validation",
    mixins = [
        "has_native_resultdb_integration",
    ],
    binary = "webgpu_blink_web_tests",
)

targets.tests.gpu_telemetry_test(
    name = "webgpu_cts_compat_tests",
    telemetry_test_name = "webgpu_compat_cts",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgpu_cts_fxc_tests",
    telemetry_test_name = "webgpu_cts",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgpu_cts_fxc_with_validation_tests",
    telemetry_test_name = "webgpu_cts",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgpu_cts_tests",
    telemetry_test_name = "webgpu_cts",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgpu_cts_service_worker_tests",
    telemetry_test_name = "webgpu_cts",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--use-worker=service",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgpu_cts_dedicated_worker_tests",
    telemetry_test_name = "webgpu_cts",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--use-worker=dedicated",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgpu_cts_shared_worker_tests",
    telemetry_test_name = "webgpu_cts",
    mixins = [
        "has_native_resultdb_integration",
    ],
    args = [
        "--use-worker=shared",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgpu_cts_with_validation_tests",
    telemetry_test_name = "webgpu_cts",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.isolated_script_test(
    name = "webgpu_swiftshader_blink_web_tests",
    mixins = [
        "has_native_resultdb_integration",
    ],
    binary = "webgpu_blink_web_tests",
)

targets.tests.isolated_script_test(
    name = "webgpu_swiftshader_blink_web_tests_with_backend_validation",
    mixins = [
        "has_native_resultdb_integration",
    ],
    binary = "webgpu_blink_web_tests",
)

targets.tests.gpu_telemetry_test(
    name = "webgpu_swiftshader_web_platform_cts_tests",
    telemetry_test_name = "webgpu_cts",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.gpu_telemetry_test(
    name = "webgpu_swiftshader_web_platform_cts_with_validation_tests",
    telemetry_test_name = "webgpu_cts",
    mixins = [
        "has_native_resultdb_integration",
    ],
)

targets.tests.script_test(
    name = "webkit_lint",
    script = "blink_lint_expectations.py",
)

targets.tests.gtest_test(
    name = "webkit_unit_tests",
    binary = "blink_unittests",
)

targets.tests.isolated_script_test(
    name = "webview_blink_wpt_tests",
    args = [
    ],
    binary = "trichrome_webview_wpt_64",
)

targets.tests.gtest_test(
    name = "webview_cts_tests",
    mixins = [
        "webview_cts_archive",
    ],
)

targets.tests.gtest_test(
    name = "webview_cts_tests_bfcache_mutations",
    mixins = [
        "webview_cts_archive",
    ],
    args = [
        "--use-apk-under-test-flags-file",
        "--enable-features=WebViewBackForwardCache",
    ],
    binary = "webview_cts_tests",
)

targets.tests.gtest_test(
    name = "webview_cts_tests_no_field_trial",
    mixins = [
        "webview_cts_archive",
    ],
    args = [
        "--disable-field-trial-config",
    ],
    binary = "webview_cts_tests",
)

targets.tests.gtest_test(
    name = "webview_trichrome_cts_tests",
    mixins = [
        "webview_cts_archive",
    ],
)

targets.tests.gtest_test(
    name = "webview_trichrome_64_32_cts_tests",
    mixins = [
        "webview_cts_archive",
    ],
)

targets.tests.gtest_test(
    name = "webview_trichrome_64_cts_tests",
    mixins = [
        "webview_cts_archive",
    ],
)

targets.tests.gtest_test(
    name = "webview_trichrome_64_cts_hostside_tests",
    mixins = [
        "webview_cts_archive",
    ],
)

targets.tests.gtest_test(
    name = "webview_trichrome_64_cts_tests_no_field_trial",
    mixins = [
        "webview_cts_archive",
    ],
    args = [
        "--disable-field-trial-config",
    ],
    binary = "webview_trichrome_64_cts_tests",
)

targets.tests.gtest_test(
    name = "webview_64_cts_tests",
    mixins = [
        "webview_cts_archive",
    ],
)

targets.tests.gtest_test(
    name = "webview_instrumentation_test_apk",
)

# This target is only to run on Android versions <= Android Q (10).
targets.tests.gtest_test(
    name = "webview_instrumentation_test_apk_single_process_mode",
    args = [
        "--webview-process-mode=single",
    ],
    binary = "webview_instrumentation_test_apk",
)

targets.tests.gtest_test(
    name = "webview_instrumentation_test_apk_multiple_process_mode",
    args = [
        "--webview-process-mode=multiple",
    ],
    binary = "webview_instrumentation_test_apk",
)

targets.tests.gtest_test(
    name = "webview_instrumentation_test_apk_mutations",
    args = [
        "--use-apk-under-test-flags-file",
        "--webview-mutations-enabled",
    ],
    binary = "webview_instrumentation_test_apk",
)

targets.tests.gtest_test(
    name = "webview_instrumentation_test_apk_bfcache_mutations",
    args = [
        "--use-apk-under-test-flags-file",
        "--enable-features=WebViewBackForwardCache",
    ],
    binary = "webview_instrumentation_test_apk",
)

targets.tests.gtest_test(
    name = "webview_instrumentation_test_apk_no_field_trial",
    args = [
        "--disable-field-trial-config",
    ],
    binary = "webview_instrumentation_test_apk",
)

targets.tests.gtest_test(
    name = "webview_ui_test_app_test_apk",
)

targets.tests.gtest_test(
    name = "webview_ui_test_app_test_apk_no_field_trial",
    args = [
        "--disable-field-trial-config",
    ],
    binary = "webview_ui_test_app_test_apk",
)

targets.tests.gtest_test(
    name = "wm_unittests",
)

targets.tests.isolated_script_test(
    name = "wpt_tests_ios",
    binary = "chrome_ios_wpt",
)

targets.tests.gtest_test(
    name = "wtf_unittests",
)

targets.tests.isolated_script_test(
    name = "xr.webxr.static",
    args = [
        "--benchmarks=xr.webxr.static",
        "-v",
        "--upload-results",
        "--output-format=histograms",
        "--browser=release_x64",
    ],
    binary = "vr_perf_tests",
)

targets.tests.gtest_test(
    name = "xr_browser_tests",
)

targets.tests.isolated_script_test(
    name = "xvfb_py_unittests",
)

targets.tests.gtest_test(
    name = "zlib_unittests",
)

targets.tests.gtest_test(
    name = "zucchini_unittests",
)
