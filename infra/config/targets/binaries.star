# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/targets.star", "targets")

targets.binaries.console_test_launcher(
    name = "absl_hardening_tests",
    label = "//third_party/abseil-cpp:absl_hardening_tests",
)

targets.binaries.console_test_launcher(
    name = "accessibility_unittests",
    label = "//ui/accessibility:accessibility_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "android_browsertests",
    label = "//chrome/test:android_browsertests",
)

targets.binaries.windowed_test_launcher(
    name = "android_sync_integration_tests",
    label = "//chrome/test:android_sync_integration_tests",
)

targets.binaries.generated_script(
    name = "android_webview_junit_tests",
    label = "//android_webview/test:android_webview_junit_tests",
)

targets.binaries.console_test_launcher(
    name = "android_webview_unittests",
    label = "//android_webview/test:android_webview_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_egl_tests",
    label = "//third_party/angle/src/tests:angle_deqp_egl_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_gles2_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles2_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_gles31_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles31_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_gles3_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles3_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_khr_gles2_tests",
    label = "//third_party/angle/src/tests:angle_deqp_khr_gles2_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_khr_gles3_tests",
    label = "//third_party/angle/src/tests:angle_deqp_khr_gles3_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_khr_gles31_tests",
    label = "//third_party/angle/src/tests:angle_deqp_khr_gles31_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_gles3_rotate180_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles3_rotate180_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_gles3_rotate270_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles3_rotate270_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_gles3_rotate90_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles3_rotate90_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_gles31_rotate180_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles31_rotate180_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_gles31_rotate270_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles31_rotate270_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_deqp_gles31_rotate90_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles31_rotate90_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_end2end_tests",
    label = "//third_party/angle/src/tests:angle_end2end_tests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_unittests",
    label = "//third_party/angle/src/tests:angle_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "angle_white_box_tests",
    label = "//third_party/angle/src/tests:angle_white_box_tests",
)

targets.binaries.windowed_test_launcher(
    name = "app_shell_unittests",
    label = "//extensions/shell:app_shell_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "ash_components_unittests",
    label = "//ash/components:ash_components_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "ash_webui_unittests",
    label = "//ash/webui:ash_webui_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "ash_unittests",
    label = "//ash:ash_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "ash_pixeltests",
    label = "//ash:ash_pixeltests",
)

targets.binaries.windowed_test_launcher(
    name = "aura_unittests",
    label = "//ui/aura:aura_unittests",
)

targets.binaries.generated_script(
    name = "base_junit_tests",
    label = "//base:base_junit_tests",
)

targets.binaries.script(
    name = "base_perftests",
    label = "//base:base_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    skip_usage_check = True,  # Used by Pinpoint: crbug.com/1042778
    args = [
        "base_perftests",
        "--non-telemetry=true",
        "--test-launcher-print-test-stdio=always",
        "--test-launcher-jobs=1",
        "--test-launcher-retry-limit=0",
    ],
)

targets.binaries.console_test_launcher(
    name = "base_unittests",
    label = "//base:base_unittests",
)

targets.binaries.console_test_launcher(
    name = "blink_common_unittests",
    label = "//third_party/blink/common:blink_common_unittests",
)

targets.binaries.console_test_launcher(
    name = "blink_fuzzer_unittests",
    label = "//third_party/blink/renderer/platform:blink_fuzzer_unittests",
)

targets.binaries.console_test_launcher(
    name = "blink_heap_unittests",
    label = "//third_party/blink/renderer/platform/heap:blink_heap_unittests",
)

targets.binaries.console_test_launcher(
    name = "blink_platform_unittests",
    label = "//third_party/blink/renderer/platform:blink_platform_unittests",
)

targets.binaries.generated_script(
    name = "blink_python_tests",
    label = "//:blink_python_tests",
    resultdb = targets.resultdb(
        enable = True,
    ),
)

targets.binaries.script(
    name = "blink_pytype",
    label = "//third_party/blink/tools:blink_pytype",
    script = "//third_party/blink/tools/run_pytype.py",
)

targets.binaries.console_test_launcher(
    name = "blink_unittests",
    label = "//third_party/blink/renderer/controller:blink_unittests",
)

targets.binaries.generated_script(
    name = "blink_web_tests",
    label = "//:blink_web_tests",
    results_handler = "layout tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
    merge = targets.merge(
        script = "//third_party/blink/tools/merge_web_test_results.py",
        args = [
            "--verbose",
        ],
    ),
)

targets.binaries.generated_script(
    name = "blink_wpt_tests",
    label = "//:blink_wpt_tests",
    results_handler = "layout tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
    merge = targets.merge(
        script = "//third_party/blink/tools/merge_web_test_results.py",
        args = [
            "--verbose",
        ],
    ),
)

targets.binaries.console_test_launcher(
    name = "boringssl_crypto_tests",
    label = "//third_party/boringssl:boringssl_crypto_tests",
)

targets.binaries.console_test_launcher(
    name = "boringssl_ssl_tests",
    label = "//third_party/boringssl:boringssl_ssl_tests",
)

targets.binaries.windowed_test_launcher(
    name = "browser_tests",
    label = "//chrome/test:browser_tests",
)

# TODO(b/246519185) - Py3 incompatible, decide if to keep test.
# targets.binaries.windowed_test_launcher(
#     name = "browser_tests_apprtc",
#     label = "//chrome/test:browser_tests_apprtc",
#     executable = "browser_tests",
# )

targets.binaries.generated_script(
    name = "build_junit_tests",
    label = "//build/android:build_junit_tests",
)

targets.binaries.windowed_test_launcher(
    name = "captured_sites_interactive_tests",
    label = "//chrome/test:captured_sites_interactive_tests",
    args = [
        "--disable-extensions",
    ],
)

targets.binaries.windowed_test_launcher(
    name = "capture_unittests",
    label = "//media/capture:capture_unittests",
)

targets.binaries.console_test_launcher(
    name = "cast_runner_browsertests",
    label = "//fuchsia_web/runners:cast_runner_browsertests",
)

targets.binaries.console_test_launcher(
    name = "cast_runner_integration_tests",
    label = "//fuchsia_web/runners:cast_runner_integration_tests",
)

targets.binaries.console_test_launcher(
    name = "cast_runner_unittests",
    label = "//fuchsia_web/runners:cast_runner_unittests",
)

# TODO(issues.chromium.org/1516671): Remove unneeded cast_* suites.

targets.binaries.console_test_launcher(
    name = "cast_android_cma_backend_unittests",
    label = "//chromecast/media/cma/backend/android:cast_android_cma_backend_unittests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "cast_audio_backend_unittests",
    label = "//chromecast/media/cma/backend:cast_audio_backend_unittests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "cast_base_unittests",
    label = "//chromecast/base:cast_base_unittests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "cast_cast_core_unittests",
    label = "//chromecast/cast_core:cast_cast_core_unittests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "cast_crash_unittests",
    label = "//chromecast/crash:cast_crash_unittests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "cast_display_settings_unittests",
    label = "//chromecast/ui/display_settings:cast_display_settings_unittests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "cast_graphics_unittests",
    label = "//chromecast/graphics:cast_graphics_unittests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "cast_media_unittests",
    label = "//chromecast/media:cast_media_unittests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "cast_shell_browsertests",
    label = "//chromecast:cast_shell_browsertests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "cast_shell_unittests",
    label = "//chromecast:cast_shell_unittests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.windowed_test_launcher(
    name = "cast_unittests",
    label = "//media/cast:cast_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "cc_unittests",
    label = "//cc:cc_unittests",
)

targets.binaries.generated_script(
    name = "chrome_all_tast_tests",
    label = "//chromeos:chrome_all_tast_tests",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.binaries.console_test_launcher(
    name = "chrome_app_unittests",
    label = "//chrome/test:chrome_app_unittests",
)

targets.binaries.generated_script(
    name = "chrome_criticalstaging_tast_tests",
    label = "//chromeos:chrome_criticalstaging_tast_tests",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.binaries.generated_script(
    name = "chrome_disabled_tast_tests",
    label = "//chromeos:chrome_disabled_tast_tests",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.binaries.console_test_launcher(
    name = "chrome_elf_unittests",
    label = "//chrome/chrome_elf:chrome_elf_unittests",
)

targets.binaries.generated_script(
    name = "chrome_java_test_pagecontroller_junit_tests",
    label = "//chrome/test/android:chrome_java_test_pagecontroller_junit_tests",
)

targets.binaries.console_test_launcher(
    name = "chrome_java_test_wpr_tests",
    label = "//chrome/test/android:chrome_java_test_wpr_tests",
)

targets.binaries.generated_script(
    name = "chrome_junit_tests",
    label = "//chrome/android:chrome_junit_tests",
)

targets.binaries.console_test_launcher(
    name = "chrome_ml_unittests",
    label = "//components/optimization_guide/internal:chrome_ml_unittests",
)

targets.binaries.generated_script(
    name = "chrome_private_code_test",
    label = "//chrome:chrome_private_code_test",
)

targets.binaries.console_test_launcher(
    name = "chrome_public_apk_baseline_profile_generator",
    label = "//chrome/test/android:chrome_public_apk_baseline_profile_generator",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "chrome_public_smoke_test",
    label = "//chrome/android:chrome_public_smoke_test",
)

# TODO(crbug.com/40193330): Rename to chrome_public_integration_test_apk
targets.binaries.console_test_launcher(
    name = "chrome_public_test_apk",
    label = "//chrome/android:chrome_public_test_apk",
)

targets.binaries.console_test_launcher(
    name = "chrome_public_test_vr_apk",
    label = "//chrome/android:chrome_public_test_vr_apk",
)

targets.binaries.console_test_launcher(
    name = "chrome_public_unit_test_apk",
    label = "//chrome/android:chrome_public_unit_test_apk",
)

targets.binaries.generated_script(
    name = "chrome_public_wpt",
    label = "//chrome/android:chrome_public_wpt",
    # All references have been moved to starlark
    skip_usage_check = True,
    results_handler = "layout tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
    merge = targets.merge(
        script = "//third_party/blink/tools/merge_web_test_results.py",
        args = [
            "--verbose",
        ],
    ),
)

targets.binaries.generated_script(
    name = "chrome_sizes",
    label = "//chrome/test:chrome_sizes",
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
    ),
)

targets.binaries.script(
    name = "chromedriver_py_tests",
    label = "//chrome/test/chromedriver:chromedriver_py_tests",
    script = "//testing/xvfb.py",
    args = [
        "../../testing/scripts/run_chromedriver_tests.py",
        "../../chrome/test/chromedriver/test/run_py_tests.py",
        "--chromedriver=chromedriver",
        "--log-path=${ISOLATED_OUTDIR}/chrome.chromedriver.log",
        "--browser-name=chrome",
    ],
    resultdb = targets.resultdb(
        enable = True,
    ),
)

targets.binaries.script(
    name = "chromedriver_py_tests_headless_shell",
    label = "//chrome/test/chromedriver:chromedriver_py_tests_headless_shell",
    script = "//testing/scripts/run_chromedriver_tests.py",
    args = [
        "../../chrome/test/chromedriver/test/run_py_tests.py",
        "--chromedriver=chromedriver",
        "--log-path=${ISOLATED_OUTDIR}/chrome-headless-shell.chromedriver.log",
        "--browser-name=chrome-headless-shell",
    ],
    resultdb = targets.resultdb(
        enable = True,
    ),
)

targets.binaries.windowed_test_launcher(
    name = "chromeos_integration_tests",
    label = "//chrome/test:chromeos_integration_tests",
)

targets.binaries.generated_script(
    name = "chrome_wpt_tests",
    label = "//:chrome_wpt_tests",
    results_handler = "layout tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
    merge = targets.merge(
        script = "//third_party/blink/tools/merge_web_test_results.py",
        args = [
            "--verbose",
        ],
    ),
)

targets.binaries.generated_script(
    name = "chrome_ios_wpt",
    label = "//ios/chrome/test/wpt:chrome_ios_wpt",
    results_handler = "layout tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
    merge = targets.merge(
        script = "//third_party/blink/tools/merge_web_test_results.py",
        args = [
            "--verbose",
        ],
    ),
)

targets.binaries.script(
    name = "chromedriver_replay_unittests",
    label = "//chrome/test/chromedriver:chromedriver_replay_unittests",
    script = "//chrome/test/chromedriver/log_replay/client_replay_unittest.py",
)

targets.binaries.windowed_test_launcher(
    name = "chromedriver_unittests",
    label = "//chrome/test/chromedriver:chromedriver_unittests",
)

targets.binaries.console_test_launcher(
    name = "chromeos_components_unittests",
    label = "//chromeos/components:chromeos_components_unittests",
)

targets.binaries.console_test_launcher(
    name = "chromeos_unittests",
    label = "//chromeos:chromeos_unittests",
)

targets.binaries.script(
    name = "command_buffer_perftests",
    label = "//gpu:command_buffer_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "command_buffer_perftests",
        "--non-telemetry=true",
        "--adb-path",
        "src/third_party/android_sdk/public/platform-tools/adb",
    ],
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
        args = [
            "--smoke-test-mode",
        ],
    ),
)

targets.binaries.windowed_test_launcher(
    name = "components_browsertests",
    label = "//components:components_browsertests",
)

targets.binaries.generated_script(
    name = "components_junit_tests",
    label = "//components:components_junit_tests",
)

targets.binaries.script(
    name = "components_perftests",
    label = "//components:components_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "--xvfb",
        "--non-telemetry=true",
        "components_perftests",
    ],
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
        args = [
            "--smoke-test-mode",
        ],
    ),
)

targets.binaries.windowed_test_launcher(
    name = "components_unittests",
    label = "//components:components_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "compositor_unittests",
    label = "//ui/compositor:compositor_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "content_browsertests",
    label = "//content/test:content_browsertests",
)

targets.binaries.generated_script(
    name = "content_junit_tests",
    label = "//content/public/android:content_junit_tests",
)

targets.binaries.script(
    name = "content_shell_crash_test",
    label = "//content/shell:content_shell_crash_test",
    script = "//testing/scripts/content_shell_crash_test.py",
    resultdb = targets.resultdb(
        enable = True,
        result_format = "single",
    ),
)

targets.binaries.console_test_launcher(
    name = "content_shell_test_apk",
    label = "//content/shell/android:content_shell_test_apk",
)

targets.binaries.windowed_test_launcher(
    name = "content_unittests",
    label = "//content/test:content_unittests",
)

targets.binaries.console_test_launcher(
    name = "crashpad_tests",
    label = "//third_party/crashpad/crashpad:crashpad_tests",
)

targets.binaries.console_test_launcher(
    name = "cronet_sample_test_apk",
    label = "//components/cronet/android:cronet_sample_test_apk",
)

targets.binaries.generated_script(
    name = "cronet_sizes",
    label = "//components/cronet/android:cronet_sizes",
    # All references have been moved to starlark
    skip_usage_check = True,
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
    ),
    resultdb = targets.resultdb(
        enable = True,
        result_format = "single",
        result_file = "${ISOLATED_OUTDIR}/sizes/test_results.json",
    ),
)

targets.binaries.console_test_launcher(
    name = "cronet_smoketests_apk",
    label = "//components/cronet/android:cronet_smoketests_apk",
)

targets.binaries.console_test_launcher(
    name = "cronet_smoketests_missing_native_library_instrumentation_apk",
    label = "//components/cronet/android:cronet_smoketests_missing_native_library_instrumentation_apk",
)

targets.binaries.console_test_launcher(
    name = "cronet_smoketests_platform_only_instrumentation_apk",
    label = "//components/cronet/android:cronet_smoketests_platform_only_instrumentation_apk",
)

targets.binaries.console_test_launcher(
    name = "cronet_test_instrumentation_apk",
    label = "//components/cronet/android:cronet_test_instrumentation_apk",
)

targets.binaries.console_test_launcher(
    name = "cronet_tests",
    label = "//components/cronet:cronet_tests",
)

targets.binaries.console_test_launcher(
    name = "cronet_tests_android",
    label = "//components/cronet/android:cronet_tests_android",
)

targets.binaries.console_test_launcher(
    name = "cronet_unittests",
    label = "//components/cronet:cronet_unittests",
)

targets.binaries.console_test_launcher(
    name = "cronet_unittests_android",
    label = "//components/cronet/android:cronet_unittests_android",
)

targets.binaries.console_test_launcher(
    name = "crypto_unittests",
    label = "//crypto:crypto_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "dawn_end2end_tests",
    label = "//third_party/dawn/src/dawn/tests:dawn_end2end_tests",
)

targets.binaries.script(
    name = "dawn_perf_tests",
    label = "//third_party/dawn/src/dawn/tests:dawn_perf_tests",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "dawn_perf_tests",
        "--non-telemetry=true",
        "--test-launcher-print-test-stdio=always",
        "--test-launcher-jobs=1",
        "--test-launcher-retry-limit=0",
    ],
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
        args = [
            # Does not upload to the perf dashboard
            "--smoke-test-mode",
        ],
    ),
)

targets.binaries.windowed_test_launcher(
    name = "dbus_unittests",
    label = "//dbus:dbus_unittests",
)

targets.binaries.console_test_launcher(
    name = "delayloads_unittests",
    label = "//chrome/test:delayloads_unittests",
)

targets.binaries.generated_script(
    name = "device_junit_tests",
    label = "//device:device_junit_tests",
)

targets.binaries.console_test_launcher(
    name = "device_unittests",
    label = "//device:device_unittests",
)

targets.binaries.generated_script(
    name = "disk_usage_tast_test",
    label = "//chromeos:disk_usage_tast_test",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
    ),
)

targets.binaries.console_test_launcher(
    name = "display_unittests",
    label = "//ui/display:display_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "elevated_tracing_service_unittests",
    label = "//chrome/windows_services/elevated_tracing_service:elevated_tracing_service_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "elevation_service_unittests",
    label = "//chrome/elevation_service:elevation_service_unittests",
)

targets.binaries.script(
    name = "enterprise_companion_integration_tests",
    label = "//chrome/enterprise_companion:enterprise_companion_integration_tests",
    script = "//testing/scripts/run_telemetry_as_googletest.py",
    args = [
        "test_service/enterprise_companion_integration_tests_launcher.py",
        "--test-output-dir=${ISOLATED_OUTDIR}",
        "--test-launcher-bot-mode",
        "--gtest_shuffle",
    ],
)

targets.binaries.console_test_launcher(
    name = "enterprise_companion_tests",
    label = "//chrome/enterprise_companion:enterprise_companion_tests",
    args = ["--gtest_shuffle"],
)

targets.binaries.console_test_launcher(
    name = "env_chromium_unittests",
    label = "//third_party/leveldatabase:env_chromium_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "events_unittests",
    label = "//ui/events:events_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "exo_unittests",
    label = "//components/exo:exo_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "extensions_browsertests",
    label = "//extensions:extensions_browsertests",
)

targets.binaries.windowed_test_launcher(
    name = "extensions_unittests",
    label = "//extensions:extensions_unittests",
)

targets.binaries.console_test_launcher(
    name = "fake_libva_driver_unittest",
    label = "//media/gpu/vaapi/test/fake_libva_driver:fake_libva_driver_unittest",
)

targets.binaries.generated_script(
    name = "headless_shell_wpt",
    label = "//:headless_shell_wpt",
    results_handler = "layout tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
    merge = targets.merge(
        script = "//third_party/blink/tools/merge_web_test_results.py",
        args = [
            "--verbose",
        ],
    ),
)

targets.binaries.console_test_launcher(
    name = "video_decode_accelerator_tests",
    label = "//media/gpu/test:video_decode_accelerator_tests",
)

targets.binaries.console_test_launcher(
    name = "filesystem_service_unittests",
    label = "//components/services/filesystem:filesystem_service_unittests",
)

targets.binaries.script(
    name = "flatbuffers_unittests",
    label = "//third_party/flatbuffers:flatbuffers_unittests",
    script = "//testing/scripts/run_flatbuffers_unittests.py",
    resultdb = targets.resultdb(
        enable = True,
        result_format = "single",
    ),
)

targets.binaries.script(
    name = "fuchsia_pytype",
    label = "//testing:fuchsia_pytype",
    script = "//build/fuchsia/test/run_pytype.py",
)

targets.binaries.generated_script(
    name = "fuchsia_sizes",
    label = "//tools/fuchsia/size_tests:fuchsia_sizes",
    # All references have been moved to starlark
    skip_usage_check = True,
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
    ),
)

targets.binaries.console_test_launcher(
    name = "fuzzing_unittests",
    label = "//testing/libfuzzer/tests:fuzzing_unittests",
)

targets.binaries.console_test_launcher(
    name = "gcm_unit_tests",
    label = "//google_apis/gcm:gcm_unit_tests",
)

targets.binaries.console_test_launcher(
    name = "gcp_unittests",
    label = "//chrome/credential_provider/test:gcp_unittests",
)

targets.binaries.console_test_launcher(
    name = "gfx_unittests",
    label = "//ui/gfx:gfx_unittests",
)

targets.binaries.console_test_launcher(
    name = "gin_unittests",
    label = "//gin:gin_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "gl_tests",
    label = "//gpu:gl_tests",
    args = [],
)

targets.binaries.windowed_test_launcher(
    name = "gl_unittests",
    label = "//ui/gl:gl_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "gl_unittests_ozone",
    label = "//ui/gl:gl_unittests_ozone",
    label_type = "group",
    executable = "gl_unittests",
)

targets.binaries.script(
    name = "gold_common_pytype",
    label = "//build:gold_common_pytype",
    script = "//build/skia_gold_common/run_pytype.py",
)

targets.binaries.console_test_launcher(
    name = "google_apis_unittests",
    label = "//google_apis:google_apis_unittests",
)

targets.binaries.script(
    name = "gpu_perftests",
    label = "//gpu:gpu_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    skip_usage_check = True,  # Used by Pinpoint: crbug.com/1042778
    args = [
        "gpu_perftests",
        "--non-telemetry=true",
        "--adb-path",
        "src/third_party/android_sdk/public/platform-tools/adb",
    ],
)

targets.binaries.script(
    name = "gpu_pytype",
    label = "//content/test:gpu_pytype",
    script = "//content/test/gpu/run_pytype.py",
)

targets.binaries.windowed_test_launcher(
    name = "gpu_unittests",
    label = "//gpu:gpu_unittests",
)

targets.binaries.script(
    name = "grit_python_unittests",
    label = "//tools/grit:grit_python_unittests",
    script = "//testing/scripts/run_isolated_script_test.py",
    args = [
        "../../tools/grit/grit/test_suite_all.py",
    ],
    resultdb = targets.resultdb(
        enable = True,
    ),
)

targets.binaries.console_test_launcher(
    name = "gwp_asan_unittests",
    label = "//components/gwp_asan:gwp_asan_unittests",
)

targets.binaries.console_test_launcher(
    name = "headless_browsertests",
    label = "//headless:headless_browsertests",
)

targets.binaries.console_test_launcher(
    name = "headless_unittests",
    label = "//headless:headless_unittests",
)

targets.binaries.console_test_launcher(
    name = "install_static_unittests",
    label = "//chrome/install_static:install_static_unittests",
)

targets.binaries.console_test_launcher(
    name = "installer_util_unittests",
    label = "//chrome/installer/util:installer_util_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "interactive_ui_tests",
    label = "//chrome/test:interactive_ui_tests",
    args = [
        "--snapshot-output-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.binaries.generated_script(
    name = "ios_chrome_unittests",
    label = "//ios/chrome/test:ios_chrome_unittests",
)

targets.binaries.generated_script(
    name = "ios_chrome_bookmarks_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_bookmarks_eg2tests_module",
)

targets.binaries.generated_script(
    name = "ios_chrome_integration_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_integration_eg2tests_module",
)

targets.binaries.generated_script(
    name = "ios_chrome_settings_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_settings_eg2tests_module",
)

targets.binaries.generated_script(
    name = "ios_chrome_signin_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_signin_eg2tests_module",
)

targets.binaries.generated_script(
    name = "ios_chrome_smoke_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_smoke_eg2tests_module",
)

targets.binaries.generated_script(
    name = "ios_chrome_ui_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_ui_eg2tests_module",
)

targets.binaries.generated_script(
    name = "ios_chrome_web_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_web_eg2tests_module",
)

targets.binaries.generated_script(
    name = "ios_crash_xcuitests_module",
    label = "//third_party/crashpad/crashpad/test/ios:ios_crash_xcuitests_module",
)

targets.binaries.generated_script(
    name = "ios_components_unittests",
    label = "//ios/components:ios_components_unittests",
)

targets.binaries.generated_script(
    name = "ios_net_unittests",
    label = "//ios/net:ios_net_unittests",
)

targets.binaries.generated_script(
    name = "ios_testing_unittests",
    label = "//ios/testing:ios_testing_unittests",
)

targets.binaries.generated_script(
    name = "ios_web_inttests",
    label = "//ios/web:ios_web_inttests",
)

targets.binaries.generated_script(
    name = "ios_web_shell_eg2tests_module",
    label = "//ios/web/shell/test:ios_web_shell_eg2tests_module",
)

targets.binaries.generated_script(
    name = "ios_web_unittests",
    label = "//ios/web:ios_web_unittests",
)

targets.binaries.generated_script(
    name = "ios_web_view_inttests",
    label = "//ios/web_view:ios_web_view_inttests",
)

targets.binaries.generated_script(
    name = "ios_web_view_unittests",
    label = "//ios/web_view:ios_web_view_unittests",
)

targets.binaries.console_test_launcher(
    name = "ipc_tests",
    label = "//ipc:ipc_tests",
)

targets.binaries.generated_script(
    name = "junit_unit_tests",
    label = "//testing/android/junit:junit_unit_tests",
)

targets.binaries.windowed_test_launcher(
    name = "keyboard_unittests",
    label = "//ash/keyboard/ui:keyboard_unittests",
)

targets.binaries.console_test_launcher(
    name = "latency_unittests",
    label = "//ui/latency:latency_unittests",
)

targets.binaries.console_test_launcher(
    name = "leveldb_unittests",
    label = "//third_party/leveldatabase:leveldb_unittests",
)

targets.binaries.console_test_launcher(
    name = "libcups_unittests",
    label = "//chrome/services/cups_proxy:libcups_unittests",
)

targets.binaries.console_test_launcher(
    name = "libjingle_xmpp_unittests",
    label = "//third_party/libjingle_xmpp:libjingle_xmpp_unittests",
)

targets.binaries.console_test_launcher(
    name = "liburlpattern_unittests",
    label = "//third_party/liburlpattern:liburlpattern_unittests",
)

targets.binaries.script(
    name = "load_library_perf_tests",
    label = "//chrome/test:load_library_perf_tests",
    script = "//testing/scripts/run_performance_tests.py",
    skip_usage_check = True,  # Used by Pinpoint: crbug.com/1042778
    args = [
        "load_library_perf_tests",
        "--non-telemetry=true",
        "--test-launcher-print-test-stdio=always",
    ],
)

targets.binaries.generated_script(
    name = "mac_signing_tests",
    label = "//chrome/installer/mac:mac_signing_tests",
)

targets.binaries.generated_script(
    name = "media_base_junit_tests",
    label = "//media/base/android:media_base_junit_tests",
)

targets.binaries.script(
    name = "media_perftests",
    label = "//media:media_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    skip_usage_check = True,  # Used by Pinpoint: crbug.com/1042778
    args = [
        "media_perftests",
        "--non-telemetry=true",
        "--single-process-tests",
        "--test-launcher-retry-limit=0",
        "--isolated-script-test-filter=*::-*_unoptimized::*_unaligned::*unoptimized_aligned",
    ],
)

targets.binaries.script(
    name = "media_router_e2e_tests",
    label = "//chrome/test/media_router:media_router_e2e_tests",
    script = "//chrome/test/media_router/internal/media_router_tests.py",
    args = [
        "--test_binary",
        "./interactive_ui_tests",
    ],
)

targets.binaries.windowed_test_launcher(
    name = "media_unittests",
    label = "//media:media_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "message_center_unittests",
    label = "//ui/message_center:message_center_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "midi_unittests",
    label = "//media/midi:midi_unittests",
)

targets.binaries.script(
    name = "mini_installer_tests",
    label = "//chrome/test/mini_installer:mini_installer_tests",
    script = "//testing/scripts/run_isolated_script_test.py",
    args = [
        "../../chrome/test/mini_installer/run_mini_installer_tests.py",
        "--output-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.binaries.console_test_launcher(
    name = "minidump_uploader_test",
    label = "//components/minidump_uploader:minidump_uploader_test",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "jni_zero_sample_apk_test",
    label = "//third_party/jni_zero/sample:jni_zero_sample_apk_test",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.generated_script(
    name = "model_validation_tests",
    label = "//components/optimization_guide/internal/testing:model_validation_tests",
)

targets.binaries.generated_script(
    name = "model_validation_tests_light",
    label = "//components/optimization_guide/internal/testing:model_validation_tests_light",
)

targets.binaries.generated_script(
    name = "module_installer_junit_tests",
    label = "//components/module_installer/android:module_installer_junit_tests",
)

targets.binaries.console_test_launcher(
    name = "monochrome_public_smoke_test",
    label = "//chrome/android:monochrome_public_smoke_test",
)

targets.binaries.console_test_launcher(
    name = "monochrome_public_bundle_smoke_test",
    label = "//chrome/android:monochrome_public_bundle_smoke_test",
)

targets.binaries.script(
    name = "mojo_python_unittests",
    label = "//mojo/public/tools:mojo_python_unittests",
    script = "//testing/scripts/run_isolated_script_test.py",
    args = [
        "../../mojo/public/tools/run_all_python_unittests.py",
    ],
    resultdb = targets.resultdb(
        enable = True,
    ),
)

targets.binaries.console_test_launcher(
    name = "mojo_rust_integration_unittests",
    label = "//mojo/public/rust:mojo_rust_integration_unittests",
)

targets.binaries.console_test_launcher(
    name = "mojo_rust_unittests",
    label = "//mojo/public/rust:mojo_rust_unittests",
)

targets.binaries.console_test_launcher(
    name = "mojo_test_apk",
    label = "//mojo/public/java/system:mojo_test_apk",
)

targets.binaries.console_test_launcher(
    name = "mojo_unittests",
    label = "//mojo:mojo_unittests",
)

targets.binaries.script(
    name = "monochrome_public_apk_checker",
    label = "//chrome/android/monochrome:monochrome_public_apk_checker",
    script = "//testing/scripts/run_isolated_script_test.py",
    args = [
        "../../chrome/android/monochrome/scripts/monochrome_python_tests.py",
        "--chrome-apk",
        "apks/ChromePublic.apk",
        "--chrome-pathmap",
        "apks/ChromePublic.apk.pathmap.txt",
        "--system-webview-apk",
        "apks/SystemWebView.apk",
        "--system-webview-pathmap",
        "apks/SystemWebView.apk.pathmap.txt",
        "--monochrome-apk",
        "apks/MonochromePublic.apk",
        "--monochrome-pathmap",
        "apks/MonochromePublic.apk.pathmap.txt",
    ],
)

targets.binaries.console_test_launcher(
    name = "monochrome_public_test_ar_apk",
    label = "//chrome/android:monochrome_public_test_ar_apk",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "nacl_loader_unittests",
    label = "//components/nacl/loader:nacl_loader_unittests",
)

targets.binaries.generated_script(
    name = "build_rust_tests",
    label = "//build/rust/tests:build_rust_tests",
)

targets.binaries.windowed_test_launcher(
    name = "native_theme_unittests",
    label = "//ui/native_theme:native_theme_unittests",
)

targets.binaries.generated_script(
    name = "net_junit_tests",
    label = "//net/android:net_junit_tests",
)

targets.binaries.script(
    name = "net_perftests",
    label = "//net:net_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    skip_usage_check = True,  # Used by Pinpoint: crbug.com/1042778
    args = [
        "net_perftests",
        "--non-telemetry=true",
    ],
)

targets.binaries.console_test_launcher(
    name = "net_unittests",
    label = "//net:net_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "notification_helper_unittests",
    label = "//chrome/notification_helper:notification_helper_unittests",
)

targets.binaries.generated_script(
    name = "ondevice_quality_tests",
    label = "//components/optimization_guide/internal/testing:ondevice_quality_tests",
)

targets.binaries.generated_script(
    name = "ondevice_stability_tests",
    label = "//components/optimization_guide/internal/testing:ondevice_stability_tests",
)

targets.binaries.generated_script(
    name = "ondevice_stability_tests_light",
    label = "//components/optimization_guide/internal/testing:ondevice_stability_tests_light",
)

targets.binaries.generated_script(
    name = "chrome_ai_wpt_tests",
    label = "//components/optimization_guide/internal/testing:chrome_ai_wpt_tests",
    results_handler = "layout tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
    merge = targets.merge(
        script = "//third_party/blink/tools/merge_web_test_results.py",
        args = [
            "--verbose",
        ],
    ),
)

targets.binaries.console_test_launcher(
    name = "openscreen_unittests",
    label = "//chrome/browser/media/router:openscreen_unittests",
)

targets.binaries.console_test_launcher(
    name = "optimization_guide_gpu_unittests",
    label = "//components/optimization_guide/internal:optimization_guide_gpu_unittests",
)

targets.binaries.console_test_launcher(
    name = "optimization_guide_unittests",
    label = "//components/optimization_guide/internal:optimization_guide_unittests",
)

targets.binaries.console_test_launcher(
    name = "ozone_gl_unittests",
    label = "//ui/ozone/gl:ozone_gl_unittests",
)

targets.binaries.console_test_launcher(
    name = "ozone_unittests",
    label = "//ui/ozone:ozone_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "ozone_x11_unittests",
    label = "//ui/ozone:ozone_x11_unittests",
)

targets.binaries.generated_script(
    name = "paint_preview_junit_tests",
    label = "//components/paint_preview/player/android:paint_preview_junit_tests",
)

targets.binaries.generated_script(
    name = "password_check_junit_tests",
    label = "//chrome/browser/password_check/android:password_check_junit_tests",
)

targets.binaries.generated_script(
    name = "password_manager_junit_tests",
    label = "//chrome/browser/password_manager/android:password_manager_junit_tests",
)

targets.binaries.console_test_launcher(
    name = "pdf_unittests",
    label = "//pdf:pdf_unittests",
)

targets.binaries.console_test_launcher(
    name = "perfetto_unittests",
    label = "//third_party/perfetto:perfetto_unittests",
)

targets.binaries.script(
    name = "performance_browser_tests",
    label = "//chrome/test:performance_browser_tests",
    script = "//testing/scripts/run_performance_tests.py",
    skip_usage_check = True,  # Used by Pinpoint: crbug.com/1042778
    args = [
        "browser_tests",
        "--non-telemetry=true",
        "--full-performance-run",
        "--test-launcher-jobs=1",
        "--test-launcher-retry-limit=0",
        "--test-launcher-print-test-stdio=always",
        # Allow the full performance runs to take up to 60 seconds (rather than
        # the default of 30 for normal CQ browser test runs).
        "--ui-test-action-timeout=60000",
        "--ui-test-action-max-timeout=60000",
        "--test-launcher-timeout=60000",
        "--gtest_filter=*/TabCapturePerformanceTest.*:*/CastV2PerformanceTest.*",
    ],
)

targets.binaries.generated_script(
    name = "performance_test_suite",
    label = "//chrome/test:performance_test_suite",
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
        args = [
            "--smoke-test-mode",
        ],
    ),
)

targets.binaries.generated_script(
    name = "performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle",
    label = "//chrome/test:performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle",
)

targets.binaries.generated_script(
    name = "performance_test_suite_android_clank_trichrome_bundle",
    label = "//chrome/test:performance_test_suite_android_clank_trichrome_bundle",
)

targets.binaries.script(
    name = "performance_web_engine_test_suite",
    label = "//content/test:performance_web_engine_test_suite",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "../../content/test/gpu/run_telemetry_benchmark_fuchsia.py",
        "--per-test-logs-dir",
    ],
)

targets.binaries.script(
    name = "performance_webview_test_suite",
    label = "//chrome/test:performance_webview_test_suite",
    script = "//third_party/catapult/devil/devil/android/tools/system_app.py",
    args = [
        "remove",
        "--package",
        "com.android.webview",
        "com.google.android.webview",
        "-v",
        "--",
        "../../testing/scripts/run_performance_tests.py",
        "../../tools/perf/run_benchmark",
    ],
)

targets.binaries.script(
    name = "polymer_tools_python_unittests",
    label = "//tools/polymer:polymer_tools_python_unittests",
    script = "//testing/scripts/run_isolated_script_test.py",
    args = [
        "../../tools/polymer/run_polymer_tools_tests.py",
    ],
)

targets.binaries.console_test_launcher(
    name = "power_sampler_unittests",
    label = "//tools/mac/power:power_sampler_unittests",
)

targets.binaries.console_test_launcher(
    name = "ppapi_unittests",
    label = "//ppapi:ppapi_unittests",
)

targets.binaries.console_test_launcher(
    name = "printing_unittests",
    label = "//printing:printing_unittests",
)

targets.binaries.generated_script(
    name = "private_code_failure_test",
    label = "//build/private_code_test:private_code_failure_test",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "profile_provider_unittest",
    label = "//chrome/browser/metrics/perf:profile_provider_unittest",
)

targets.binaries.console_test_launcher(
    name = "pthreadpool_unittests",
    label = "//third_party/pthreadpool:pthreadpool_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "remoting_unittests",
    label = "//remoting:remoting_unittests",
)

targets.binaries.generated_script(
    name = "resource_sizes_cronet_sample_apk",
    label = "//components/cronet/android:resource_sizes_cronet_sample_apk",
    # All references have been moved to starlark
    skip_usage_check = True,
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
    ),
    resultdb = targets.resultdb(
        enable = True,
        result_format = "single",
    ),
)

targets.binaries.console_test_launcher(
    name = "rust_gtest_interop_unittests",
    label = "//testing/rust_gtest_interop:rust_gtest_interop_unittests",
)

targets.binaries.console_test_launcher(
    name = "sandbox_linux_unittests",
    label = "//sandbox/linux:sandbox_linux_unittests",
)

targets.binaries.console_test_launcher(
    name = "sandbox_unittests",
    label = "//sandbox:sandbox_unittests",
)

targets.binaries.console_test_launcher(
    name = "sbox_integration_tests",
    label = "//sandbox/win:sbox_integration_tests",
)

targets.binaries.console_test_launcher(
    name = "sbox_unittests",
    label = "//sandbox/win:sbox_unittests",
)

targets.binaries.console_test_launcher(
    name = "sbox_validation_tests",
    label = "//sandbox/win:sbox_validation_tests",
)

targets.binaries.generated_script(
    name = "services_junit_tests",
    label = "//services:services_junit_tests",
)

targets.binaries.windowed_test_launcher(
    name = "services_unittests",
    label = "//services:services_unittests",
)

targets.binaries.console_test_launcher(
    name = "setup_unittests",
    label = "//chrome/installer/setup:setup_unittests",
)

targets.binaries.console_test_launcher(
    name = "shell_encryption_unittests",
    label = "//third_party/shell-encryption:shell_encryption_unittests",
)

targets.binaries.console_test_launcher(
    name = "shell_dialogs_unittests",
    label = "//ui/shell_dialogs:shell_dialogs_unittests",
    # These tests are more like dialog interactive ui tests.
    args = [
        "--test-launcher-jobs=1",
    ],
)

targets.binaries.console_test_launcher(
    name = "skia_unittests",
    label = "//skia:skia_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "snapshot_unittests",
    label = "//ui/snapshot:snapshot_unittests",
)

targets.binaries.console_test_launcher(
    name = "sql_unittests",
    label = "//sql:sql_unittests",
)

targets.binaries.console_test_launcher(
    name = "storage_unittests",
    label = "//storage:storage_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "sync_integration_tests",
    label = "//chrome/test:sync_integration_tests",
)

targets.binaries.script(
    name = "sync_performance_tests",
    label = "//chrome/test:sync_performance_tests",
    script = "//testing/scripts/run_performance_tests.py",
    skip_usage_check = True,  # Used by Pinpoint: crbug.com/1042778
    args = [
        "sync_performance_tests",
        "--non-telemetry=true",
        "--test-launcher-print-test-stdio=always",
        "--test-launcher-jobs=1",
        "--test-launcher-retry-limit=0",
    ],
)

targets.binaries.console_test_launcher(
    name = "system_webview_shell_layout_test_apk",
    label = "//android_webview/tools/system_webview_shell:system_webview_shell_layout_test_apk",
)

targets.binaries.generated_script(
    name = "system_webview_wpt",
    label = "//android_webview/test:system_webview_wpt",
    # All references have been moved to starlark
    skip_usage_check = True,
    results_handler = "layout tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
    merge = targets.merge(
        script = "//third_party/blink/tools/merge_web_test_results.py",
        args = [
            "--verbose",
        ],
    ),
)

targets.binaries.generated_script(
    name = "trichrome_webview_wpt_64",
    label = "//android_webview/test:trichrome_webview_wpt_64",
    # All references have been moved to starlark
    skip_usage_check = True,
    results_handler = "layout tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
    merge = targets.merge(
        script = "//third_party/blink/tools/merge_web_test_results.py",
        args = [
            "--verbose",
        ],
    ),
)

targets.binaries.generated_script(
    name = "telemetry_gpu_integration_test",
    label = "//chrome/test:telemetry_gpu_integration_test",
)

targets.binaries.generated_script(
    name = "telemetry_gpu_integration_test_android_chrome",
    label = "//chrome/test:telemetry_gpu_integration_test_android_chrome",
)

targets.binaries.script(
    name = "telemetry_gpu_integration_test_android_webview",
    label = "//chrome/test:telemetry_gpu_integration_test_android_webview",
    script = "//testing/scripts/run_gpu_integration_test_as_googletest.py",
    args = [
        "../../content/test/gpu/run_gpu_integration_test.py",
    ],
)

targets.binaries.script(
    name = "telemetry_gpu_integration_test_fuchsia",
    label = "//content/test:telemetry_gpu_integration_test_fuchsia",
    script = "//testing/scripts/run_gpu_integration_test_as_googletest.py",
    # All references have been moved to starlark
    skip_usage_check = True,
    args = [
        "../../content/test/gpu/run_gpu_integration_test_fuchsia.py",
        "--logs-dir",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.binaries.script(
    name = "telemetry_gpu_unittests",
    label = "//chrome/test:telemetry_gpu_unittests",
    script = "//testing/scripts/run_telemetry_as_googletest.py",
    args = [
        "../../content/test/gpu/run_unittests.py",
        "-v",
    ],
    resultdb = targets.resultdb(
        enable = True,
    ),
)

# This isolate is used by
# https://www.chromium.org/developers/cluster-telemetry
targets.binaries.script(
    name = "ct_telemetry_perf_tests_without_chrome",
    label = "//chrome/test:ct_telemetry_perf_tests_without_chrome",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "../../tools/perf/run_benchmark",
    ],
)

targets.binaries.generated_script(
    name = "telemetry_perf_unittests",
    label = "//chrome/test:telemetry_perf_unittests",
)

targets.binaries.generated_script(
    name = "telemetry_perf_unittests_android_chrome",
    label = "//chrome/test:telemetry_perf_unittests_android_chrome",
)

targets.binaries.generated_script(
    name = "telemetry_perf_unittests_android_monochrome",
    label = "//chrome/test:telemetry_perf_unittests_android_monochrome",
)

targets.binaries.generated_script(
    name = "telemetry_unittests",
    label = "//chrome/test:telemetry_unittests",
)

targets.binaries.console_test_launcher(
    name = "test_cpp_including_rust_unittests",
    label = "//build/rust/tests/test_cpp_including_rust:test_cpp_including_rust_unittests",
)

targets.binaries.generated_script(
    name = "test_env_py_unittests",
    label = "//testing:test_env_py_unittests",
    resultdb = targets.resultdb(
        enable = True,
    ),
)

targets.binaries.console_test_launcher(
    name = "test_serde_json_lenient",
    label = "//build/rust/tests/test_serde_json_lenient:test_serde_json_lenient",
)

targets.binaries.script(
    name = "testing_pytype",
    label = "//testing:testing_pytype",
    script = "//testing/run_pytype.py",
)

targets.binaries.generated_script(
    name = "touch_to_fill_junit_tests",
    label = "//chrome/browser/touch_to_fill/password_manager/android:touch_to_fill_junit_tests",
)

targets.binaries.script(
    name = "tracing_perftests",
    label = "//components/tracing:tracing_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    skip_usage_check = True,  # Used by Pinpoint: crbug.com/1042778
    args = [
        "tracing_perftests",
        "--non-telemetry=true",
        "--test-launcher-print-test-stdio=always",
        "--adb-path",
        "src/third_party/android_sdk/public/platform-tools/adb",
    ],
)

targets.binaries.console_test_launcher(
    name = "trichrome_chrome_bundle_smoke_test",
    label = "//chrome/android:trichrome_chrome_bundle_smoke_test",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.binaries.console_test_launcher(
    name = "ui_android_unittests",
    label = "//ui/android:ui_android_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "ui_base_unittests",
    label = "//ui/base:ui_base_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "ui_chromeos_unittests",
    label = "//ui/chromeos:ui_chromeos_unittests",
)

targets.binaries.generated_script(
    name = "ui_junit_tests",
    label = "//ui:ui_junit_tests",
)

targets.binaries.windowed_test_launcher(
    name = "ui_touch_selection_unittests",
    label = "//ui/touch_selection:ui_touch_selection_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "unit_tests",
    label = "//chrome/test:unit_tests",
)

# The test action timeouts for `updater_tests`, `updater_tests_system`, and
# `updater_tests_win_uac` are based on empirical observations of test
# runtimes, 2021-07. The launcher timeout was 90000 but then we increased
# the value to 180000 to work around an unfixable issue in the Windows
# COM runtime class activation crbug.com/1259178.
targets.binaries.console_test_launcher(
    name = "updater_tests",
    label = "//chrome/updater:updater_tests",
    args = [
        "--gtest_shuffle",
        "--test-launcher-timeout=180000",
        "--ui-test-action-max-timeout=45000",
        "--ui-test-action-timeout=40000",
    ],
)

targets.binaries.console_test_launcher(
    name = "updater_tests_system",
    label = "//chrome/updater:updater_tests_system",
    args = [
        "--gtest_shuffle",
        "--test-launcher-timeout=180000",
        "--ui-test-action-max-timeout=45000",
        "--ui-test-action-timeout=40000",
        "--exclude-paths-from-win-defender",
    ],
)

targets.binaries.script(
    name = "updater_tests_win_uac",
    label = "//chrome/updater:updater_tests_win_uac",
    script = "//testing/scripts/run_telemetry_as_googletest.py",
    args = [
        "test_service/run_command_as_standard_user.py",
        "--command=updater_tests.exe",
        "--test-launcher-bot-mode",
        "--cfi-diag=0",
        "--gtest_shuffle",
        "--test-launcher-timeout=180000",
        "--ui-test-action-max-timeout=45000",
        "--ui-test-action-timeout=40000",
    ],
)

targets.binaries.console_test_launcher(
    name = "ui_unittests",
    label = "//ui/tests:ui_unittests",
)

targets.binaries.generated_script(
    name = "upload_trace_processor",
    label = "//tools/perf/core/perfetto_binary_roller:upload_trace_processor",
)

targets.binaries.console_test_launcher(
    name = "url_unittests",
    label = "//url:url_unittests",
)

targets.binaries.console_test_launcher(
    name = "usage_time_limit_unittests",
    label = "//chrome/browser/ash/child_accounts/time_limit_consistency_test:usage_time_limit_unittests",
)

targets.binaries.console_test_launcher(
    name = "vaapi_unittest",
    label = "//media/gpu/vaapi:vaapi_unittest",
)

targets.binaries.generated_script(
    name = "variations_desktop_smoke_tests",
    label = "//chrome/test/variations:variations_desktop_smoke_tests",
)

targets.binaries.generated_script(
    name = "variations_smoke_tests",
    label = "//chrome/test:variations_smoke_tests",
    resultdb = targets.resultdb(
        enable = True,
        result_format = "single",
    ),
)

targets.binaries.windowed_test_launcher(
    name = "views_examples_unittests",
    label = "//ui/views/examples:views_examples_unittests",
)

targets.binaries.script(
    name = "views_perftests",
    label = "//ui/views:views_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "--xvfb",
        "--non-telemetry=true",
        "views_perftests",
    ],
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
        args = [
            "--smoke-test-mode",
        ],
    ),
)

targets.binaries.windowed_test_launcher(
    name = "views_unittests",
    label = "//ui/views:views_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "viz_unittests",
    label = "//components/viz:viz_unittests",
)

targets.binaries.console_test_launcher(
    name = "vr_android_unittests",
    label = "//chrome/browser/android/vr:vr_android_unittests",
)

targets.binaries.script(
    name = "vr_common_perftests",
    label = "//chrome/browser/vr:vr_common_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "vr_common_perftests",
        "--non-telemetry=true",
        "--adb-path",
        "src/third_party/android_sdk/public/platform-tools/adb",
    ],
)

targets.binaries.console_test_launcher(
    name = "vr_common_unittests",
    label = "//chrome/browser/vr:vr_common_unittests",
)

targets.binaries.script(
    name = "vr_perf_tests",
    label = "//tools/perf/contrib/vr_benchmarks:vr_perf_tests",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "../../tools/perf/run_benchmark",
    ],
    merge = targets.merge(
        script = "//tools/perf/process_perf_results.py",
    ),
)

targets.binaries.script(
    name = "vrcore_fps_test",
    label = "//chrome/test/vr/perf:vrcore_fps_test",
    script = "//chrome/test/vr/perf/vrcore_fps/run_vrcore_fps_test.py",
    args = [
        "-v",
    ],
)

targets.binaries.windowed_test_launcher(
    name = "vulkan_tests",
    label = "//gpu/vulkan:vulkan_tests",
)

targets.binaries.windowed_test_launcher(
    name = "wayland_client_perftests",
    label = "//components/exo/wayland:wayland_client_perftests",
)

targets.binaries.windowed_test_launcher(
    name = "wayland_client_tests",
    label = "//components/exo/wayland:wayland_client_tests",
)

targets.binaries.console_test_launcher(
    name = "web_engine_browsertests",
    label = "//fuchsia_web/webengine:web_engine_browsertests",
)

targets.binaries.console_test_launcher(
    name = "web_engine_integration_tests",
    label = "//fuchsia_web/webengine:web_engine_integration_tests",
)

targets.binaries.console_test_launcher(
    name = "web_engine_unittests",
    label = "//fuchsia_web/webengine:web_engine_unittests",
)

targets.binaries.generated_script(
    name = "webapk_client_junit_tests",
    label = "//chrome/android/webapk/libs/client:webapk_client_junit_tests",
)

targets.binaries.generated_script(
    name = "webapk_shell_apk_h2o_junit_tests",
    label = "//chrome/android/webapk/shell_apk:webapk_shell_apk_h2o_junit_tests",
)

targets.binaries.generated_script(
    name = "webapk_shell_apk_junit_tests",
    label = "//chrome/android/webapk/shell_apk:webapk_shell_apk_junit_tests",
)

targets.binaries.generated_script(
    name = "webgpu_blink_web_tests",
    label = "//:webgpu_blink_web_tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.binaries.script(
    name = "webview_cts_tests",
    label = "//android_webview/test:webview_cts_tests",
    script = "//android_webview/tools/run_cts.py",
    args = [
        "--skip-expected-failures",
        "--use-webview-provider",
        "apks/SystemWebView.apk",
        "--apk-under-test",
        "apks/SystemWebView.apk",
        "--use-apk-under-test-flags-file",
        "-v",
        # Required for stack.py to find build artifacts for symbolization.
        "--output-directory",
        ".",
    ],
)

targets.binaries.script(
    name = "webview_trichrome_cts_tests",
    label = "//android_webview/test:webview_trichrome_cts_tests",
    script = "//android_webview/tools/run_cts.py",
    # All references have been moved to starlark
    skip_usage_check = True,
    args = [
        "--skip-expected-failures",
        "--additional-apk",
        "apks/TrichromeLibrary.apk",
        "--use-webview-provider",
        "apks/TrichromeWebView.apk",
        "--apk-under-test",
        "apks/TrichromeWebView.apk",
        "--use-apk-under-test-flags-file",
        "-v",
        # Required for stack.py to find build artifacts for symbolization.
        "--output-directory",
        ".",
    ],
)

targets.binaries.script(
    name = "webview_trichrome_64_32_cts_tests",
    label = "//android_webview/test:webview_trichrome_64_32_cts_tests",
    script = "//android_webview/tools/run_cts.py",
    # All references have been moved to starlark
    skip_usage_check = True,
    args = [
        "--skip-expected-failures",
        "--additional-apk",
        "apks/TrichromeLibrary6432.apk",
        "--use-webview-provider",
        "apks/TrichromeWebView6432.apk",
        "--apk-under-test",
        "apks/TrichromeWebView6432.apk",
        "--use-apk-under-test-flags-file",
        "-v",
        # Required for stack.py to find build artifacts for symbolization.
        "--output-directory",
        ".",
    ],
)

targets.binaries.script(
    name = "webview_trichrome_64_cts_tests",
    label = "//android_webview/test:webview_trichrome_64_cts_tests",
    script = "//android_webview/tools/run_cts.py",
    args = [
        "--skip-expected-failures",
        "--additional-apk",
        "apks/TrichromeLibrary64.apk",
        "--use-webview-provider",
        "apks/TrichromeWebView64.apk",
        "--apk-under-test",
        "apks/TrichromeWebView64.apk",
        "--use-apk-under-test-flags-file",
        "-v",
        # Required for stack.py to find build artifacts for symbolization.
        "--output-directory",
        ".",
    ],
)

targets.binaries.script(
    name = "webview_trichrome_64_cts_hostside_tests",
    label = "//android_webview/test:webview_trichrome_64_cts_hostside_tests",
    script = "//android_webview/tools/run_cts.py",
    # All references have been moved to starlark
    skip_usage_check = True,
    args = [
        "--cts-gcs-path",
        "../../android_webview/tools/cts_config/webview_cts_hostside_gcs_path.json",
        "--skip-expected-failures",
        "--additional-apk",
        "apks/TrichromeLibrary64.apk",
        "--use-webview-provider",
        "apks/TrichromeWebView64.apk",
        "--module-apk",
        "CtsHostsideWebViewTests.apk",
    ],
)

targets.binaries.script(
    name = "webview_64_cts_tests",
    label = "//android_webview/test:webview_64_cts_tests",
    script = "//android_webview/tools/run_cts.py",
    # All references have been moved to starlark
    skip_usage_check = True,
    args = [
        "--skip-expected-failures",
        "--use-webview-provider",
        "apks/SystemWebView64.apk",
        "--apk-under-test",
        "apks/SystemWebView64.apk",
        "--use-apk-under-test-flags-file",
        "-v",
        # Required for stack.py to find build artifacts for symbolization.
        "--output-directory",
        ".",
    ],
)

targets.binaries.console_test_launcher(
    name = "webview_instrumentation_test_apk",
    label = "//android_webview/test:webview_instrumentation_test_apk",
)

targets.binaries.console_test_launcher(
    name = "webview_ui_test_app_test_apk",
    label = "//android_webview/tools/automated_ui_tests:webview_ui_test_app_test_apk",
)

targets.binaries.windowed_test_launcher(
    name = "wm_unittests",
    label = "//ui/wm:wm_unittests",
)

targets.binaries.console_test_launcher(
    name = "wtf_unittests",
    label = "//third_party/blink/renderer/platform/wtf:wtf_unittests",
)

targets.binaries.windowed_test_launcher(
    name = "xr_browser_tests",
    label = "//chrome/test:xr_browser_tests",
    # We can't use the "script" type since we need to be run from the output
    # directory (or at least given the path). Thus, we need to tell mb.py to not
    # automatically append the .exe suffix on Windows.
    executable = "run_xr_browser_tests.py",
    executable_suffix = "",
    args = [
        "--enable-gpu",
        "--test-launcher-bot-mode",
        "--test-launcher-jobs=1",
        "--test-launcher-retry-limit=0",
        "--enable-pixel-output-in-tests",
        "--enable-unsafe-swiftshader",
    ],
)

targets.binaries.generated_script(
    name = "xvfb_py_unittests",
    label = "//testing:xvfb_py_unittests",
    resultdb = targets.resultdb(
        enable = True,
    ),
)

targets.binaries.console_test_launcher(
    name = "zlib_unittests",
    label = "//third_party/zlib:zlib_unittests",
    args = [
        "--test-launcher-timeout=400000",
    ],
)

targets.binaries.console_test_launcher(
    name = "zucchini_unittests",
    label = "//components/zucchini:zucchini_unittests",
)
