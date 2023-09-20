# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/targets.star", "targets")

targets.console_test_launcher(
    name = "absl_hardening_tests",
    label = "//third_party/abseil-cpp:absl_hardening_tests",
)

targets.console_test_launcher(
    name = "accessibility_unittests",
    label = "//ui/accessibility:accessibility_unittests",
)

targets.windowed_test_launcher(
    name = "android_browsertests",
    label = "//chrome/test:android_browsertests",
)

targets.compile_target(
    name = "android_lint",
    label = "//chrome/android:android_lint",
)

targets.windowed_test_launcher(
    name = "android_sync_integration_tests",
    label = "//chrome/test:android_sync_integration_tests",
)

targets.compile_target(
    name = "android_tools",
    label = "//tools/android:android_tools",
)

targets.generated_script(
    name = "android_webview_junit_tests",
    label = "//android_webview/test:android_webview_junit_tests",
)

targets.console_test_launcher(
    name = "android_webview_unittests",
    label = "//android_webview/test:android_webview_unittests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_egl_tests",
    label = "//third_party/angle/src/tests:angle_deqp_egl_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_gles2_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles2_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_gles31_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles31_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_gles3_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles3_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_khr_gles2_tests",
    label = "//third_party/angle/src/tests:angle_deqp_khr_gles2_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_khr_gles3_tests",
    label = "//third_party/angle/src/tests:angle_deqp_khr_gles3_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_khr_gles31_tests",
    label = "//third_party/angle/src/tests:angle_deqp_khr_gles31_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_gles3_rotate180_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles3_rotate180_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_gles3_rotate270_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles3_rotate270_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_gles3_rotate90_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles3_rotate90_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_gles31_rotate180_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles31_rotate180_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_gles31_rotate270_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles31_rotate270_tests",
)

targets.windowed_test_launcher(
    name = "angle_deqp_gles31_rotate90_tests",
    label = "//third_party/angle/src/tests:angle_deqp_gles31_rotate90_tests",
)

targets.windowed_test_launcher(
    name = "angle_end2end_tests",
    label = "//third_party/angle/src/tests:angle_end2end_tests",
)

targets.windowed_test_launcher(
    name = "angle_unittests",
    label = "//third_party/angle/src/tests:angle_unittests",
)

targets.windowed_test_launcher(
    name = "angle_white_box_tests",
    label = "//third_party/angle/src/tests:angle_white_box_tests",
)

targets.windowed_test_launcher(
    name = "app_shell_unittests",
    label = "//extensions/shell:app_shell_unittests",
)

targets.windowed_test_launcher(
    name = "ash_components_unittests",
    label = "//ash/components:ash_components_unittests",
)

targets.windowed_test_launcher(
    name = "ash_crosapi_tests",
    label = "//chrome/test:ash_crosapi_tests",
)

targets.windowed_test_launcher(
    name = "ash_webui_unittests",
    label = "//ash/webui:ash_webui_unittests",
)

targets.windowed_test_launcher(
    name = "ash_unittests",
    label = "//ash:ash_unittests",
)

targets.windowed_test_launcher(
    name = "ash_pixeltests",
    label = "//ash:ash_pixeltests",
)

targets.windowed_test_launcher(
    name = "aura_unittests",
    label = "//ui/aura:aura_unittests",
)

targets.generated_script(
    name = "base_junit_tests",
    label = "//base:base_junit_tests",
)

targets.compile_target(
    name = "base_nocompile_tests",
    label = "//base:base_nocompile_tests",
)

targets.script(
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

targets.console_test_launcher(
    name = "base_unittests",
    label = "//base:base_unittests",
)

targets.console_test_launcher(
    name = "blink_common_unittests",
    label = "//third_party/blink/common:blink_common_unittests",
)

targets.console_test_launcher(
    name = "blink_fuzzer_unittests",
    label = "//third_party/blink/renderer/platform:blink_fuzzer_unittests",
)

targets.console_test_launcher(
    name = "blink_heap_unittests",
    label = "//third_party/blink/renderer/platform/heap:blink_heap_unittests",
)

targets.compile_target(
    name = "blink_platform_nocompile_tests",
    label = "//third_party/blink/renderer/platform:blink_platform_nocompile_tests",
)

targets.console_test_launcher(
    name = "blink_platform_unittests",
    label = "//third_party/blink/renderer/platform:blink_platform_unittests",
)

targets.compile_target(
    name = "blink_probes_nocompile_tests",
    label = "//third_party/blink/renderer/core/probe:blink_probes_nocompile_tests",
)

targets.generated_script(
    name = "blink_python_tests",
    label = "//:blink_python_tests",
)

targets.script(
    name = "blink_pytype",
    label = "//third_party/blink/tools:blink_pytype",
    script = "//third_party/blink/tools/run_pytype.py",
)

targets.compile_target(
    name = "blink_tests",
    label = "//:blink_tests",
)

targets.console_test_launcher(
    name = "blink_unittests",
    label = "//third_party/blink/renderer/controller:blink_unittests",
)

targets.generated_script(
    name = "blink_web_tests",
    label = "//:blink_web_tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.generated_script(
    name = "blink_wpt_tests",
    label = "//:blink_wpt_tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
)

# TODO(b/246519185) - Py3 incompatible, decide if to keep test.
# targets.windowed_test_launcher(
#     name = "browser_tests_apprtc",
#     label = "//chrome/test:browser_tests_apprtc",
#     executable = "browser_tests",
# )

targets.console_test_launcher(
    name = "boringssl_crypto_tests",
    label = "//third_party/boringssl:boringssl_crypto_tests",
)

targets.console_test_launcher(
    name = "boringssl_ssl_tests",
    label = "//third_party/boringssl:boringssl_ssl_tests",
)

targets.windowed_test_launcher(
    name = "browser_tests",
    label = "//chrome/test:browser_tests",
)

#   # TODO(b/246519185) - Py3 incompatible, decide if to keep test.
#   #"browser_tests_apprtc": {
#   #  label = "//chrome/test:browser_tests_apprtc",
#   #  type = "windowed_test_launcher",
#   #  executable = "browser_tests",
#   )

targets.generated_script(
    name = "build_junit_tests",
    label = "//build/android:build_junit_tests",
)

targets.windowed_test_launcher(
    name = "captured_sites_interactive_tests",
    label = "//chrome/test:captured_sites_interactive_tests",
    args = [
        "--disable-extensions",
    ],
)

targets.windowed_test_launcher(
    name = "capture_unittests",
    label = "//media/capture:capture_unittests",
)

targets.console_test_launcher(
    name = "cast_display_settings_unittests",
    label = "//chromecast/ui/display_settings:cast_display_settings_unittests",
)

targets.console_test_launcher(
    name = "cast_runner_browsertests",
    label = "//fuchsia_web/runners:cast_runner_browsertests",
)

targets.console_test_launcher(
    name = "cast_runner_integration_tests",
    label = "//fuchsia_web/runners:cast_runner_integration_tests",
)

targets.compile_target(
    name = "cast_runner_pkg",
    label = "//fuchsia_web/runners:cast_runner_pkg",
)

targets.console_test_launcher(
    name = "cast_runner_unittests",
    label = "//fuchsia_web/runners:cast_runner_unittests",
)

targets.console_test_launcher(
    name = "cast_audio_backend_unittests",
    label = "//chromecast/media/cma/backend:cast_audio_backend_unittests",
)

targets.junit_test(
    name = "cast_base_junit_tests",
    label = "//chromecast/base:cast_base_junit_tests",
)

targets.console_test_launcher(
    name = "cast_base_unittests",
    label = "//chromecast/base:cast_base_unittests",
)

targets.console_test_launcher(
    name = "cast_cast_core_unittests",
    label = "//chromecast/cast_core:cast_cast_core_unittests",
)

targets.console_test_launcher(
    name = "cast_crash_unittests",
    label = "//chromecast/crash:cast_crash_unittests",
)

targets.console_test_launcher(
    name = "cast_graphics_unittests",
    label = "//chromecast/graphics:cast_graphics_unittests",
)

targets.compile_target(
    name = "cast_junit_test_lists",
    label = "//chromecast:cast_junit_test_lists",
)

targets.console_test_launcher(
    name = "cast_media_unittests",
    label = "//chromecast/media:cast_media_unittests",
)

targets.compile_target(
    name = "cast_shell",
    label = "//chromecast:cast_shell",
)

targets.compile_target(
    name = "cast_shell_apk",
    label = "//chromecast:cast_shell_apk",
)

targets.console_test_launcher(
    name = "cast_shell_browsertests",
    label = "//chromecast:cast_shell_browsertests",
)

targets.junit_test(
    name = "cast_shell_junit_tests",
    label = "//chromecast/browser/android:cast_shell_junit_tests",
)

targets.console_test_launcher(
    name = "cast_shell_unittests",
    label = "//chromecast:cast_shell_unittests",
)

targets.compile_target(
    name = "cast_test_lists",
    label = "//chromecast:cast_test_lists",
)

targets.windowed_test_launcher(
    name = "cast_unittests",
    label = "//media/cast:cast_unittests",
)

targets.windowed_test_launcher(
    name = "cc_unittests",
    label = "//cc:cc_unittests",
)

targets.compile_target(
    name = "chrome",
    label = "//chrome:chrome",
)

targets.generated_script(
    name = "chrome_all_tast_tests",
    label = "//chromeos:chrome_all_tast_tests",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.console_test_launcher(
    name = "chrome_app_unittests",
    label = "//chrome/test:chrome_app_unittests",
)

targets.generated_script(
    name = "chrome_criticalstaging_tast_tests",
    label = "//chromeos:chrome_criticalstaging_tast_tests",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.generated_script(
    name = "chrome_disabled_tast_tests",
    label = "//chromeos:chrome_disabled_tast_tests",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.console_test_launcher(
    name = "chrome_elf_unittests",
    label = "//chrome/chrome_elf:chrome_elf_unittests",
)

targets.generated_script(
    name = "chrome_java_test_pagecontroller_junit_tests",
    label = "//chrome/test/android:chrome_java_test_pagecontroller_junit_tests",
)

targets.console_test_launcher(
    name = "chrome_java_test_wpr_tests",
    label = "//chrome/test/android:chrome_java_test_wpr_tests",
)

targets.generated_script(
    name = "chrome_junit_tests",
    label = "//chrome/android:chrome_junit_tests",
)

targets.compile_target(
    name = "chrome_nocompile_tests",
    label = "//chrome/android:chrome_nocompile_tests",
)

targets.compile_target(
    name = "chrome_official_builder",
    label = "//:chrome_official_builder",
)

targets.compile_target(
    name = "chrome_official_builder_no_unittests",
    label = "//:chrome_official_builder_no_unittests",
)

targets.compile_target(
    name = "chrome_pkg",
    label = "//chrome/app:chrome_pkg",
)

targets.generated_script(
    name = "chrome_private_code_test",
    label = "//chrome:chrome_private_code_test",
)

targets.console_test_launcher(
    name = "chrome_public_apk_baseline_profile_generator",
    label = "//chrome/test/android:chrome_public_apk_baseline_profile_generator",
)

targets.compile_target(
    name = "chrome_public_apk",
    label = "//chrome/android:chrome_public_apk",
)

targets.console_test_launcher(
    name = "chrome_public_smoke_test",
    label = "//chrome/android:chrome_public_smoke_test",
)

# TODO(crbug.com/1238057): Rename to chrome_public_integration_test_apk
targets.console_test_launcher(
    name = "chrome_public_test_apk",
    label = "//chrome/android:chrome_public_test_apk",
)

targets.console_test_launcher(
    name = "chrome_public_test_vr_apk",
    label = "//chrome/android:chrome_public_test_vr_apk",
)

targets.console_test_launcher(
    name = "chrome_public_unit_test_apk",
    label = "//chrome/android:chrome_public_unit_test_apk",
)

targets.generated_script(
    name = "chrome_public_wpt",
    label = "//chrome/android:chrome_public_wpt",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.compile_target(
    name = "chrome_sandbox",
    label = "//sandbox/linux:chrome_sandbox",
)

targets.generated_script(
    name = "chrome_sizes",
    label = "//chrome/test:chrome_sizes",
)

targets.compile_target(
    name = "chromedriver",
    label = "//chrome/test/chromedriver:chromedriver_server",
)

targets.compile_target(
    name = "chromedriver_group",
    label = "//:chromedriver_group",
)

targets.script(
    name = "chromedriver_py_tests",
    label = "//chrome/test/chromedriver:chromedriver_py_tests",
    script = "//testing/xvfb.py",
    args = [
        "../../testing/scripts/run_chromedriver_tests.py",
        "../../chrome/test/chromedriver/test/run_py_tests.py",
        "--chromedriver=chromedriver",
        "--log-path=${ISOLATED_OUTDIR}/chromedriver.log",
    ],
)

targets.compile_target(
    name = "chromedriver_webview_shell_apk",
    label = "//chrome/test/chromedriver/test/webview_shell:chromedriver_webview_shell_apk",
)

targets.windowed_test_launcher(
    name = "chromeos_integration_tests",
    label = "//chrome/test:chromeos_integration_tests",
)

targets.generated_script(
    name = "chrome_wpt_tests",
    label = "//:chrome_wpt_tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.generated_script(
    name = "content_shell_wpt",
    label = "//:content_shell_wpt",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.generated_script(
    name = "chrome_ios_wpt",
    label = "//ios/chrome/test/wpt:chrome_ios_wpt",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.compile_target(
    name = "chrome/browser/media/router",
    label = "//chrome/browser/media/router:router",
)

targets.compile_target(
    name = "chrome/browser/media/router:openscreen_unittests",
    label = "//chrome/browser/media/router:openscreen_unittests",
)

targets.compile_target(
    name = "chrome/browser/media/router:unittests",
    label = "//chrome/browser/media/router:unittests",
)

targets.compile_target(
    name = "chrome/installer/linux",
    label = "//chrome/installer/linux:linux",
)

targets.compile_target(
    name = "chrome/installer/mac",
    label = "//chrome/installer/mac:mac",
)

targets.generated_script(
    name = "variations_smoke_tests",
    label = "//chrome/test:variations_smoke_tests",
)

targets.generated_script(
    name = "variations_desktop_smoke_tests",
    label = "//chrome/test/variations:variations_desktop_smoke_tests",
)

targets.script(
    name = "chromedriver_replay_unittests",
    label = "//chrome/test/chromedriver:chromedriver_replay_unittests",
    script = "//chrome/test/chromedriver/log_replay/client_replay_unittest.py",
)

targets.windowed_test_launcher(
    name = "chromedriver_unittests",
    label = "//chrome/test/chromedriver:chromedriver_unittests",
)

targets.generated_script(
    name = "chrome_finch_smoke_tests",
    label = "//clank/java:chrome_finch_smoke_tests",
)

targets.console_test_launcher(
    name = "chromeos_components_unittests",
    label = "//chromeos/components:chromeos_components_unittests",
)

targets.console_test_launcher(
    name = "chromeos_unittests",
    label = "//chromeos:chromeos_unittests",
)

targets.compile_target(
    name = "chromium_builder_asan",
    label = "//:chromium_builder_asan",
)

targets.compile_target(
    name = "chromium_builder_perf",
    label = "//:chromium_builder_perf",
)

targets.compile_target(
    name = "chromiumos_preflight",
    label = "//:chromiumos_preflight",
)

targets.script(
    name = "command_buffer_perftests",
    label = "//gpu:command_buffer_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "command_buffer_perftests",
        "--non-telemetry=true",
        "--adb-path",
        "src/third_party/android_sdk/public/platform-tools/adb",
    ],
)

targets.compile_target(
    name = "components/media_router/common/providers/cast/channel",
    label = "//components/media_router/common/providers/cast/channel:cast_channel",
)

targets.compile_target(
    name = "components/media_router/common/providers/cast/channel:unit_tests",
    label = "//components/media_router/common/providers/cast/channel:unit_tests",
)

targets.compile_target(
    name = "components/media_router/common/providers/cast/certificate",
    label = "//components/media_router/common/providers/cast/certificate",
)

targets.compile_target(
    name = "components/media_router/common/providers/cast/certificate:unit_tests",
    label = "//components/media_router/common/providers/cast/certificate:unit_tests",
)

targets.compile_target(
    name = "components/mirroring/browser",
    label = "//components/mirroring/browser:browser",
)

targets.compile_target(
    name = "components/mirroring/service:mirroring_service",
    label = "//components/mirroring/service:mirrroring_service",
)

targets.compile_target(
    name = "components/mirroring:mirroring_tests",
    label = "//components/mirroring:mirroring_tests",
)

targets.compile_target(
    name = "components/mirroring:mirroring_unittests",
    label = "//components/mirroring:mirroring_unittests",
)

targets.compile_target(
    name = "components/openscreen_platform",
    label = "//components/openscreen_platform",
)

targets.windowed_test_launcher(
    name = "components_browsertests",
    label = "//components:components_browsertests",
)

targets.generated_script(
    name = "components_junit_tests",
    label = "//components:components_junit_tests",
)

targets.script(
    name = "components_perftests",
    label = "//components:components_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "--xvfb",
        "--non-telemetry=true",
        "components_perftests",
    ],
)

targets.windowed_test_launcher(
    name = "components_unittests",
    label = "//components:components_unittests",
)

targets.windowed_test_launcher(
    name = "compositor_unittests",
    label = "//ui/compositor:compositor_unittests",
)

targets.windowed_test_launcher(
    name = "content_browsertests",
    label = "//content/test:content_browsertests",
)

targets.generated_script(
    name = "content_junit_tests",
    label = "//content/public/android:content_junit_tests",
)

targets.compile_target(
    name = "content_nocompile_tests",
    label = "//content/test:content_nocompile_tests",
)

targets.script(
    name = "content_shell_crash_test",
    label = "//content/shell:content_shell_crash_test",
    script = "//testing/scripts/content_shell_crash_test.py",
)

targets.console_test_launcher(
    name = "content_shell_test_apk",
    label = "//content/shell/android:content_shell_test_apk",
)

targets.windowed_test_launcher(
    name = "content_unittests",
    label = "//content/test:content_unittests",
)

targets.compile_target(
    name = "core_runtime_simple",
    label = "//chromecast/cast_core:core_runtime_simple",
)

targets.console_test_launcher(
    name = "courgette_unittests",
    label = "//courgette:courgette_unittests",
)

targets.console_test_launcher(
    name = "crashpad_tests",
    label = "//third_party/crashpad/crashpad:crashpad_tests",
)

targets.compile_target(
    name = "cronet_package",
    label = "//components/cronet:cronet_package",
)

targets.compile_target(
    name = "cronet_perf_test_apk",
    label = "//components/cronet/android:cronet_perf_test_apk",
)

targets.console_test_launcher(
    name = "cronet_sample_test_apk",
    label = "//components/cronet/android:cronet_sample_test_apk",
)

targets.generated_script(
    name = "cronet_sizes",
    label = "//components/cronet/android:cronet_sizes",
)

targets.console_test_launcher(
    name = "cronet_smoketests_missing_native_library_instrumentation_apk",
    label = "//components/cronet/android:cronet_smoketests_missing_native_library_instrumentation_apk",
)

targets.console_test_launcher(
    name = "cronet_smoketests_platform_only_instrumentation_apk",
    label = "//components/cronet/android:cronet_smoketests_platform_only_instrumentation_apk",
)

targets.generated_script(
    name = "cronet_test",
    label = "//components/cronet/ios/test:cronet_test",
)

targets.console_test_launcher(
    name = "cronet_test_instrumentation_apk",
    label = "//components/cronet/android:cronet_test_instrumentation_apk",
)

targets.console_test_launcher(
    name = "cronet_tests",
    label = "//components/cronet:cronet_tests",
)

targets.console_test_launcher(
    name = "cronet_tests_android",
    label = "//components/cronet/android:cronet_tests_android",
)

targets.console_test_launcher(
    name = "cronet_unittests",
    label = "//components/cronet:cronet_unittests",
)

targets.console_test_launcher(
    name = "cronet_unittests_android",
    label = "//components/cronet/android:cronet_unittests_android",
)

targets.console_test_launcher(
    name = "crypto_unittests",
    label = "//crypto:crypto_unittests",
)

targets.windowed_test_launcher(
    name = "dawn_end2end_tests",
    label = "//third_party/dawn/src/dawn/tests:dawn_end2end_tests",
)

targets.script(
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
)

targets.windowed_test_launcher(
    name = "dbus_unittests",
    label = "//dbus:dbus_unittests",
)

targets.console_test_launcher(
    name = "delayloads_unittests",
    label = "//chrome/test:delayloads_unittests",
)

targets.generated_script(
    name = "device_junit_tests",
    label = "//device:device_junit_tests",
)

targets.console_test_launcher(
    name = "device_unittests",
    label = "//device:device_unittests",
)

targets.generated_script(
    name = "disk_usage_tast_test",
    label = "//chromeos:disk_usage_tast_test",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.console_test_launcher(
    name = "display_unittests",
    label = "//ui/display:display_unittests",
)

targets.windowed_test_launcher(
    name = "elevation_service_unittests",
    label = "//chrome/elevation_service:elevation_service_unittests",
)

targets.compile_target(
    name = "empty_main",
    label = "//testing:empty_main",
)

targets.console_test_launcher(
    name = "env_chromium_unittests",
    label = "//third_party/leveldatabase:env_chromium_unittests",
)

targets.windowed_test_launcher(
    name = "events_unittests",
    label = "//ui/events:events_unittests",
)

targets.windowed_test_launcher(
    name = "exo_unittests",
    label = "//components/exo:exo_unittests",
)

targets.windowed_test_launcher(
    name = "extensions_browsertests",
    label = "//extensions:extensions_browsertests",
)

targets.windowed_test_launcher(
    name = "extensions_unittests",
    label = "//extensions:extensions_unittests",
)

targets.console_test_launcher(
    name = "filesystem_service_unittests",
    label = "//components/services/filesystem:filesystem_service_unittests",
)

targets.script(
    name = "flatbuffers_unittests",
    label = "//third_party/flatbuffers:flatbuffers_unittests",
    script = "//testing/scripts/run_flatbuffers_unittests.py",
)

targets.script(
    name = "fuchsia_pytype",
    label = "//testing:fuchsia_pytype",
    script = "//build/fuchsia/test/run_pytype.py",
)

targets.generated_script(
    name = "fuchsia_sizes",
    label = "//tools/fuchsia/size_tests:fuchsia_sizes",
)

targets.console_test_launcher(
    name = "gcm_unit_tests",
    label = "//google_apis/gcm:gcm_unit_tests",
)

targets.console_test_launcher(
    name = "gcp_unittests",
    label = "//chrome/credential_provider/test:gcp_unittests",
)

targets.compile_target(
    name = "test_ash_chrome_cipd_yaml",
    label = "//chrome/test:test_ash_chrome_cipd_yaml",
)

targets.console_test_launcher(
    name = "gfx_unittests",
    label = "//ui/gfx:gfx_unittests",
)

targets.console_test_launcher(
    name = "gin_unittests",
    label = "//gin:gin_unittests",
)

targets.windowed_test_launcher(
    name = "gl_tests",
    label = "//gpu:gl_tests",
    args = [],
)

targets.windowed_test_launcher(
    name = "gl_unittests",
    label = "//ui/gl:gl_unittests",
)

targets.windowed_test_launcher(
    name = "gl_unittests_ozone",
    label = "//ui/gl:gl_unittests_ozone",
    label_type = "group",
    executable = "gl_unittests",
)

targets.console_test_launcher(
    name = "gles2_conform_test",
    label = "//gpu/gles2_conform_support:gles2_conform_test",
)

targets.compile_target(
    name = "gn_all",
    label = "//:gn_all",
)

targets.script(
    name = "gold_common_pytype",
    label = "//build:gold_common_pytype",
    script = "//build/skia_gold_common/run_pytype.py",
)

targets.console_test_launcher(
    name = "google_apis_unittests",
    label = "//google_apis:google_apis_unittests",
)

targets.script(
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

targets.script(
    name = "gpu_pytype",
    label = "//content/test:gpu_pytype",
    script = "//content/test/gpu/run_pytype.py",
)

targets.windowed_test_launcher(
    name = "gpu_unittests",
    label = "//gpu:gpu_unittests",
)

targets.script(
    name = "grit_python_unittests",
    label = "//tools/grit:grit_python_unittests",
    script = "//testing/scripts/run_isolated_script_test.py",
    args = [
        "../../tools/grit/grit/test_suite_all.py",
    ],
)

targets.console_test_launcher(
    name = "gwp_asan_unittests",
    label = "//components/gwp_asan:gwp_asan_unittests",
)

targets.console_test_launcher(
    name = "headless_browsertests",
    label = "//headless:headless_browsertests",
)

targets.console_test_launcher(
    name = "headless_unittests",
    label = "//headless:headless_unittests",
)

targets.compile_target(
    name = "image_processor_perf_test",
    label = "//media/gpu/chromeos:image_processor_perf_test",
)

targets.console_test_launcher(
    name = "install_static_unittests",
    label = "//chrome/install_static:install_static_unittests",
)

targets.console_test_launcher(
    name = "installer_util_unittests",
    label = "//chrome/installer/util:installer_util_unittests",
)

targets.windowed_test_launcher(
    name = "interactive_ui_tests",
    label = "//chrome/test:interactive_ui_tests",
    args = [
        "--snapshot-output-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.compile_target(
    name = "ios/chrome/app:chrome",
    label = "//ios/chrome/app:chrome",
)

targets.compile_target(
    name = "ios/chrome/test:all_fuzzer_tests",
    label = "//ios/chrome/test:all_fuzzer_tests",
)

targets.compile_target(
    name = "ios_chrome_clusterfuzz_asan_build",
    label = "//ios/chrome/test/wpt:ios_chrome_clusterfuzz_asan_build",
)

targets.generated_script(
    name = "ios_chrome_unittests",
    label = "//ios/chrome/test:ios_chrome_unittests",
)

targets.generated_script(
    name = "ios_chrome_bookmarks_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_bookmarks_eg2tests_module",
)

targets.generated_script(
    name = "ios_chrome_integration_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_integration_eg2tests_module",
)

targets.generated_script(
    name = "ios_chrome_settings_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_settings_eg2tests_module",
)

targets.generated_script(
    name = "ios_chrome_signin_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_signin_eg2tests_module",
)

targets.generated_script(
    name = "ios_chrome_smoke_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_smoke_eg2tests_module",
)

targets.generated_script(
    name = "ios_chrome_ui_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_ui_eg2tests_module",
)

targets.generated_script(
    name = "ios_chrome_web_eg2tests_module",
    label = "//ios/chrome/test/earl_grey2:ios_chrome_web_eg2tests_module",
)

targets.generated_script(
    name = "ios_crash_xcuitests_module",
    label = "//third_party/crashpad/crashpad/test/ios:ios_crash_xcuitests_module",
)

targets.generated_script(
    name = "ios_components_unittests",
    label = "//ios/components:ios_components_unittests",
)

targets.generated_script(
    name = "ios_net_unittests",
    label = "//ios/net:ios_net_unittests",
)

targets.generated_script(
    name = "ios_remoting_unittests",
    label = "//remoting/ios:ios_remoting_unittests",
)

targets.generated_script(
    name = "ios_showcase_eg2tests_module",
    label = "//ios/showcase:ios_showcase_eg2tests_module",
)

targets.generated_script(
    name = "ios_testing_unittests",
    label = "//ios/testing:ios_testing_unittests",
)

targets.generated_script(
    name = "ios_web_inttests",
    label = "//ios/web:ios_web_inttests",
)

targets.generated_script(
    name = "ios_web_shell_eg2tests_module",
    label = "//ios/web/shell/test:ios_web_shell_eg2tests_module",
)

targets.generated_script(
    name = "ios_web_unittests",
    label = "//ios/web:ios_web_unittests",
)

targets.generated_script(
    name = "ios_web_view_inttests",
    label = "//ios/web_view:ios_web_view_inttests",
)

targets.generated_script(
    name = "ios_web_view_unittests",
    label = "//ios/web_view:ios_web_view_unittests",
)

targets.console_test_launcher(
    name = "ipc_tests",
    label = "//ipc:ipc_tests",
)

targets.generated_script(
    name = "junit_unit_tests",
    label = "//testing/android/junit:junit_unit_tests",
)

targets.junit_test(
    name = "keyboard_accessory_junit_tests",
    label = "//chrome/android/features/keyboard_accessory:keyboard_accessory_junit_tests",
)

targets.windowed_test_launcher(
    name = "keyboard_unittests",
    label = "//ash/keyboard/ui:keyboard_unittests",
)

targets.generated_script(
    name = "lacros_all_tast_tests",
    label = "//chromeos/lacros:lacros_all_tast_tests",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.generated_script(
    name = "lacros_all_tast_tests_informational",
    label = "//chromeos/lacros:lacros_all_tast_tests_informational",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.windowed_test_launcher(
    name = "lacros_chrome_browsertests",
    label = "//chrome/test:lacros_chrome_browsertests",
)

targets.windowed_test_launcher(
    name = "lacros_chrome_browsertests_run_in_series",
    label = "//chrome/test:lacros_chrome_browsertests_run_in_series",
    args = [
        "--test-launcher-jobs=1",
    ],
)

targets.console_test_launcher(
    name = "lacros_chrome_unittests",
    label = "//chrome/test:lacros_chrome_unittests",
)

targets.generated_script(
    name = "lacros_cq_tast_tests_eve",
    label = "//chromeos/lacros:lacros_cq_tast_tests_eve",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.generated_script(
    name = "lacros_fyi_tast_tests",
    label = "//chromeos/lacros:lacros_fyi_tast_tests",
    args = [
        "--logs-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.console_test_launcher(
    name = "latency_unittests",
    label = "//ui/latency:latency_unittests",
)

targets.console_test_launcher(
    name = "leveldb_unittests",
    label = "//third_party/leveldatabase:leveldb_unittests",
)

targets.console_test_launcher(
    name = "libcups_unittests",
    label = "//chrome/services/cups_proxy:libcups_unittests",
)

targets.console_test_launcher(
    name = "libjingle_xmpp_unittests",
    label = "//third_party/libjingle_xmpp:libjingle_xmpp_unittests",
)

targets.console_test_launcher(
    name = "liburlpattern_unittests",
    label = "//third_party/liburlpattern:liburlpattern_unittests",
)

targets.compile_target(
    name = "linux_symbols",
    label = "//chrome:linux_symbols",
)

targets.script(
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

targets.generated_script(
    name = "mac_signing_tests",
    label = "//chrome/installer/mac:mac_signing_tests",
)

targets.generated_script(
    name = "media_base_junit_tests",
    label = "//media/base/android:media_base_junit_tests",
)

targets.script(
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

targets.script(
    name = "media_router_e2e_tests",
    label = "//chrome/test/media_router:media_router_e2e_tests",
    script = "//chrome/test/media_router/internal/media_router_tests.py",
    args = [
        "--test_binary",
        "./interactive_ui_tests",
    ],
)

targets.windowed_test_launcher(
    name = "media_unittests",
    label = "//media:media_unittests",
)

targets.windowed_test_launcher(
    name = "message_center_unittests",
    label = "//ui/message_center:message_center_unittests",
)

targets.windowed_test_launcher(
    name = "midi_unittests",
    label = "//media/midi:midi_unittests",
)

targets.compile_target(
    name = "mini_installer",
    label = "//chrome/installer/mini_installer:mini_installer",
)

targets.script(
    name = "mini_installer_tests",
    label = "//chrome/test/mini_installer:mini_installer_tests",
    script = "//testing/scripts/run_isolated_script_test.py",
    args = [
        "../../chrome/test/mini_installer/run_mini_installer_tests.py",
        "--output-dir=${ISOLATED_OUTDIR}",
    ],
)

targets.console_test_launcher(
    name = "minidump_uploader_test",
    label = "//components/minidump_uploader:minidump_uploader_test",
)

targets.generated_script(
    name = "model_validation_tests",
    label = "//components/optimization_guide/internal/testing:model_validation_tests",
)

targets.generated_script(
    name = "module_installer_junit_tests",
    label = "//components/module_installer/android:module_installer_junit_tests",
)

targets.generated_script(
    name = "monochrome_finch_smoke_tests",
    label = "//clank/android_webview/components:monochrome_finch_smoke_tests",
)

targets.console_test_launcher(
    name = "monochrome_public_smoke_test",
    label = "//chrome/android:monochrome_public_smoke_test",
)

targets.console_test_launcher(
    name = "monochrome_public_bundle_smoke_test",
    label = "//chrome/android:monochrome_public_bundle_smoke_test",
)

targets.script(
    name = "mojo_python_unittests",
    label = "//mojo/public/tools:mojo_python_unittests",
    script = "//testing/scripts/run_isolated_script_test.py",
    args = [
        "../../mojo/public/tools/run_all_python_unittests.py",
    ],
)

targets.compile_target(
    name = "mojo_rust",
    # Since we can't build rust tests on Android now, add this for build
    # coverage.
    label = "//mojo/public/rust:mojo_rust",
)

targets.console_test_launcher(
    name = "mojo_rust_integration_unittests",
    label = "//mojo/public/rust:mojo_rust_integration_unittests",
)

targets.console_test_launcher(
    name = "mojo_rust_unittests",
    label = "//mojo/public/rust:mojo_rust_unittests",
)

targets.console_test_launcher(
    name = "mojo_test_apk",
    label = "//mojo/public/java/system:mojo_test_apk",
)

targets.console_test_launcher(
    name = "mojo_unittests",
    label = "//mojo:mojo_unittests",
)

targets.script(
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

targets.console_test_launcher(
    name = "monochrome_public_test_ar_apk",
    label = "//chrome/android:monochrome_public_test_ar_apk",
)

targets.compile_target(
    name = "monochrome_static_initializers",
    label = "//chrome/android:monochrome_static_initializers",
)

targets.compile_target(
    name = "nacl_helper",
    label = "//components/nacl/loader:nacl_helper",
    skip_usage_check = True,
)

targets.compile_target(
    name = "nacl_helper_bootstrap",
    label = "//native_client/src/trusted/service_runtime/linux:bootstrap",
    skip_usage_check = True,
)

targets.console_test_launcher(
    name = "nacl_loader_unittests",
    label = "//components/nacl/loader:nacl_loader_unittests",
)

targets.generated_script(
    name = "build_rust_tests",
    label = "//build/rust/tests:build_rust_tests",
)

targets.windowed_test_launcher(
    name = "native_theme_unittests",
    label = "//ui/native_theme:native_theme_unittests",
)

targets.generated_script(
    name = "net_junit_tests",
    label = "//net/android:net_junit_tests",
)

targets.script(
    name = "net_perftests",
    label = "//net:net_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    skip_usage_check = True,  # Used by Pinpoint: crbug.com/1042778
    args = [
        "net_perftests",
        "--non-telemetry=true",
    ],
)

targets.console_test_launcher(
    name = "net_unittests",
    label = "//net:net_unittests",
)

targets.windowed_test_launcher(
    name = "notification_helper_unittests",
    label = "//chrome/notification_helper:notification_helper_unittests",
)

targets.console_test_launcher(
    name = "openscreen_unittests",
    label = "//chrome/browser/media/router:openscreen_unittests",
)

targets.console_test_launcher(
    name = "optimization_guide_unittests",
    label = "//components/optimization_guide/internal:optimization_guide_unittests",
)

targets.console_test_launcher(
    name = "ozone_gl_unittests",
    label = "//ui/ozone/gl:ozone_gl_unittests",
)

targets.console_test_launcher(
    name = "ozone_unittests",
    label = "//ui/ozone:ozone_unittests",
)

targets.windowed_test_launcher(
    name = "ozone_x11_unittests",
    label = "//ui/ozone:ozone_x11_unittests",
)

targets.generated_script(
    name = "paint_preview_junit_tests",
    label = "//components/paint_preview/player/android:paint_preview_junit_tests",
)

targets.generated_script(
    name = "password_check_junit_tests",
    label = "//chrome/browser/password_check/android:password_check_junit_tests",
)

targets.generated_script(
    name = "password_manager_junit_tests",
    label = "//chrome/browser/password_manager/android:password_manager_junit_tests",
)

targets.compile_target(
    name = "pdf_fuzzers",
    label = "//pdf/pdfium/fuzzers:pdf_fuzzers",
)

targets.console_test_launcher(
    name = "pdf_unittests",
    label = "//pdf:pdf_unittests",
)

targets.compile_target(
    name = "pdfium_test",
    label = "//third_party/pdfium/samples:pdfium_test",
)

targets.console_test_launcher(
    name = "perfetto_unittests",
    label = "//third_party/perfetto:perfetto_unittests",
)

targets.script(
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

targets.generated_script(
    name = "performance_test_suite",
    label = "//chrome/test:performance_test_suite",
)

targets.generated_script(
    name = "performance_test_suite_android_clank_monochrome",
    label = "//chrome/test:performance_test_suite_android_clank_monochrome",
)

targets.generated_script(
    name = "performance_test_suite_android_clank_monochrome_64_32_bundle",
    label = "//chrome/test:performance_test_suite_android_clank_monochrome_64_32_bundle",
)

targets.generated_script(
    name = "performance_test_suite_android_clank_monochrome_bundle",
    label = "//chrome/test:performance_test_suite_android_clank_monochrome_bundle",
)

targets.generated_script(
    name = "performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle",
    label = "//chrome/test:performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle",
)

targets.generated_script(
    name = "performance_test_suite_android_clank_trichrome_bundle",
    label = "//chrome/test:performance_test_suite_android_clank_trichrome_bundle",
)

targets.generated_script(
    name = "performance_test_suite_eve",
    label = "//chrome/test:performance_test_suite_eve",
)

targets.generated_script(
    name = "performance_test_suite_octopus",
    label = "//chrome/test:performance_test_suite_octopus",
)

targets.script(
    name = "performance_web_engine_test_suite",
    label = "//content/test:performance_web_engine_test_suite",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "../../content/test/gpu/run_telemetry_benchmark_fuchsia.py",
        "--per-test-logs-dir",
    ],
)

targets.script(
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

targets.script(
    name = "polymer_tools_python_unittests",
    label = "//tools/polymer:polymer_tools_python_unittests",
    script = "//testing/scripts/run_isolated_script_test.py",
    args = [
        "../../tools/polymer/run_polymer_tools_tests.py",
    ],
)

targets.compile_target(
    name = "postmortem-metadata",
    label = "//v8:postmortem-metadata",
)

targets.console_test_launcher(
    name = "power_sampler_unittests",
    label = "//tools/mac/power:power_sampler_unittests",
)

targets.console_test_launcher(
    name = "ppapi_unittests",
    label = "//ppapi:ppapi_unittests",
)

targets.compile_target(
    name = "previous_version_mini_installer",
    label = "//chrome/installer/mini_installer:previous_version_mini_installer",
)

targets.console_test_launcher(
    name = "printing_unittests",
    label = "//printing:printing_unittests",
)

targets.generated_script(
    name = "private_code_failure_test",
    label = "//build/private_code_test:private_code_failure_test",
)

targets.console_test_launcher(
    name = "profile_provider_unittest",
    label = "//chrome/browser/metrics/perf:profile_provider_unittest",
)

targets.console_test_launcher(
    name = "pthreadpool_unittests",
    label = "//third_party/pthreadpool:pthreadpool_unittests",
)

targets.compile_target(
    name = "push_apps_to_background_apk",
    label = "//tools/android/push_apps_to_background:push_apps_to_background_apk",
)

targets.compile_target(
    name = "remoting/client:client",
    label = "//remoting/client:client",
)

targets.compile_target(
    name = "remoting/host:host",
    label = "//remoting/host:host",
)

targets.windowed_test_launcher(
    name = "remoting_unittests",
    label = "//remoting:remoting_unittests",
)

targets.generated_script(
    name = "resource_sizes_chromecast",
    label = "//chromecast:resource_sizes_chromecast",
)

targets.generated_script(
    name = "resource_sizes_cronet_sample_apk",
    label = "//components/cronet/android:resource_sizes_cronet_sample_apk",
)

targets.generated_script(
    name = "resource_sizes_lacros_chrome",
    label = "//chromeos/lacros:resource_sizes_lacros_chrome",
)

targets.compile_target(
    name = "rust_build_tests",
    label = "//build/rust/tests",
)

targets.console_test_launcher(
    name = "rust_gtest_interop_unittests",
    label = "//testing/rust_gtest_interop:rust_gtest_interop_unittests",
)

targets.console_test_launcher(
    name = "sandbox_linux_unittests",
    label = "//sandbox/linux:sandbox_linux_unittests",
)

targets.console_test_launcher(
    name = "sandbox_unittests",
    label = "//sandbox:sandbox_unittests",
)

targets.console_test_launcher(
    name = "sbox_integration_tests",
    label = "//sandbox/win:sbox_integration_tests",
)

targets.console_test_launcher(
    name = "sbox_unittests",
    label = "//sandbox/win:sbox_unittests",
)

targets.console_test_launcher(
    name = "sbox_validation_tests",
    label = "//sandbox/win:sbox_validation_tests",
)

targets.generated_script(
    name = "services_junit_tests",
    label = "//services:services_junit_tests",
)

targets.console_test_launcher(
    name = "service_manager_unittests",
    label = "//services/service_manager/tests:service_manager_unittests",
)

targets.windowed_test_launcher(
    name = "services_unittests",
    label = "//services:services_unittests",
)

targets.console_test_launcher(
    name = "setup_unittests",
    label = "//chrome/installer/setup:setup_unittests",
)

targets.console_test_launcher(
    name = "shell_encryption_unittests",
    label = "//third_party/shell-encryption:shell_encryption_unittests",
)

targets.console_test_launcher(
    name = "shell_dialogs_unittests",
    label = "//ui/shell_dialogs:shell_dialogs_unittests",
    # These tests are more like dialog interactive ui tests.
    args = [
        "--test-launcher-jobs=1",
    ],
)

targets.console_test_launcher(
    name = "skia_unittests",
    label = "//skia:skia_unittests",
)

targets.windowed_test_launcher(
    name = "snapshot_unittests",
    label = "//ui/snapshot:snapshot_unittests",
)

targets.console_test_launcher(
    name = "sql_unittests",
    label = "//sql:sql_unittests",
)

targets.compile_target(
    name = "strip_lacros_files",
    label = "//chrome:strip_lacros_files",
)

targets.console_test_launcher(
    name = "storage_unittests",
    label = "//storage:storage_unittests",
)

targets.compile_target(
    name = "symupload",
    label = "//third_party/breakpad:symupload",
)

targets.windowed_test_launcher(
    name = "sync_integration_tests",
    label = "//chrome/test:sync_integration_tests",
)

targets.script(
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

targets.compile_target(
    name = "system_webview_apk",
    label = "//android_webview:system_webview_apk",
)

targets.compile_target(
    name = "system_webview_shell_apk",
    label = "//android_webview/tools/system_webview_shell:system_webview_shell_apk",
)

targets.console_test_launcher(
    name = "system_webview_shell_layout_test_apk",
    label = "//android_webview/tools/system_webview_shell:system_webview_shell_layout_test_apk",
)

targets.generated_script(
    name = "system_webview_wpt",
    label = "//android_webview/test:system_webview_wpt",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.script(
    name = "telemetry_gpu_integration_test",
    label = "//chrome/test:telemetry_gpu_integration_test",
    script = "//testing/scripts/run_gpu_integration_test_as_googletest.py",
    args = [
        "../../content/test/gpu/run_gpu_integration_test.py",
    ],
)

targets.script(
    name = "telemetry_gpu_integration_test_android_chrome",
    label = "//chrome/test:telemetry_gpu_integration_test_android_chrome",
    script = "//testing/scripts/run_gpu_integration_test_as_googletest.py",
    args = [
        "../../content/test/gpu/run_gpu_integration_test.py",
    ],
)

targets.script(
    name = "telemetry_gpu_integration_test_android_webview",
    label = "//chrome/test:telemetry_gpu_integration_test_android_webview",
    script = "//testing/scripts/run_gpu_integration_test_as_googletest.py",
    args = [
        "../../content/test/gpu/run_gpu_integration_test.py",
    ],
)

targets.script(
    name = "telemetry_gpu_integration_test_fuchsia",
    label = "//chrome/test:telemetry_gpu_integration_test_fuchsia",
    script = "//testing/scripts/run_gpu_integration_test_as_googletest.py",
    args = [
        "../../content/test/gpu/run_gpu_integration_test_fuchsia.py",
        "--logs-dir",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.compile_target(
    name = "telemetry_gpu_integration_test_scripts_only",
    label = "//chrome/test:telemetry_gpu_integration_test_scripts_only",
)

targets.script(
    name = "telemetry_gpu_unittests",
    label = "//chrome/test:telemetry_gpu_unittests",
    script = "//testing/scripts/run_telemetry_as_googletest.py",
    args = [
        "../../content/test/gpu/run_unittests.py",
        "-v",
    ],
)

# This isolate is used by
# https://www.chromium.org/developers/cluster-telemetry
targets.script(
    name = "ct_telemetry_perf_tests_without_chrome",
    label = "//chrome/test:ct_telemetry_perf_tests_without_chrome",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "../../tools/perf/run_benchmark",
    ],
)

targets.script(
    name = "telemetry_perf_unittests",
    label = "//chrome/test:telemetry_perf_unittests",
    script = "//testing/scripts/run_telemetry_as_googletest.py",
    args = [
        "../../tools/perf/run_tests",
        "-v",
    ],
)

targets.script(
    name = "telemetry_perf_unittests_android_chrome",
    label = "//chrome/test:telemetry_perf_unittests_android_chrome",
    script = "//testing/scripts/run_telemetry_as_googletest.py",
    args = [
        "../../tools/perf/run_tests",
        "-v",
    ],
)

targets.script(
    name = "telemetry_perf_unittests_android_monochrome",
    label = "//chrome/test:telemetry_perf_unittests_android_monochrome",
    script = "//testing/scripts/run_telemetry_as_googletest.py",
    args = [
        "../../tools/perf/run_tests",
        "-v",
    ],
)

targets.script(
    name = "telemetry_unittests",
    label = "//chrome/test:telemetry_unittests",
    script = "//testing/scripts/run_telemetry_as_googletest.py",
    args = [
        "--xvfb",
        "../../tools/perf/run_telemetry_tests",
        "-v",
        # TODO(nedn, eyaich): Remove this flag once crbug.com/549140 is fixed &
        # Telemetry no longer downloads files in parallel. (crbug.com/661434#c24)
        "--jobs=1",
        "--chrome-root",
        "../../",
    ],
)

targets.console_test_launcher(
    name = "test_cpp_including_rust_unittests",
    label = "//build/rust/tests/test_cpp_including_rust:test_cpp_including_rust_unittests",
)

targets.generated_script(
    name = "test_env_py_unittests",
    label = "//testing:test_env_py_unittests",
)

targets.console_test_launcher(
    name = "test_serde_json_lenient",
    label = "//build/rust/tests/test_serde_json_lenient:test_serde_json_lenient",
)

targets.script(
    name = "testing_pytype",
    label = "//testing:testing_pytype",
    script = "//testing/run_pytype.py",
)

targets.generated_script(
    name = "touch_to_fill_junit_tests",
    label = "//chrome/browser/touch_to_fill/android:touch_to_fill_junit_tests",
)

targets.compile_target(
    name = "trace_processor_shell",
    label = "//third_party/perfetto/src/trace_processor:trace_processor_shell",
)

targets.script(
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

targets.compile_target(
    name = "traffic_annotation_auditor_dependencies",
    label = "//:traffic_annotation_auditor_dependencies",
)

targets.console_test_launcher(
    name = "trichrome_chrome_bundle_smoke_test",
    label = "//chrome/android:trichrome_chrome_bundle_smoke_test",
)

targets.console_test_launcher(
    name = "ui_android_unittests",
    label = "//ui/android:ui_android_unittests",
)

targets.windowed_test_launcher(
    name = "ui_base_unittests",
    label = "//ui/base:ui_base_unittests",
)

targets.windowed_test_launcher(
    name = "ui_chromeos_unittests",
    label = "//ui/chromeos:ui_chromeos_unittests",
)

targets.generated_script(
    name = "ui_junit_tests",
    label = "//ui/android:ui_junit_tests",
)

targets.windowed_test_launcher(
    name = "ui_touch_selection_unittests",
    label = "//ui/touch_selection:ui_touch_selection_unittests",
)

targets.windowed_test_launcher(
    name = "unit_tests",
    label = "//chrome/test:unit_tests",
)

# The test action timeouts for `updater_tests`, `updater_tests_system`, and
# `updater_tests_win_uac` are based on empirical observations of test
# runtimes, 2021-07. The launcher timeout was 90000 but then we increased
# the value to 180000 to work around an unfixable issue in the Windows
# COM runtime class activation crbug.com/1259178.
targets.console_test_launcher(
    name = "updater_tests",
    label = "//chrome/updater:updater_tests",
    args = [
        "--gtest_shuffle",
        "--test-launcher-timeout=180000",
        "--ui-test-action-max-timeout=45000",
        "--ui-test-action-timeout=40000",
    ],
)

targets.console_test_launcher(
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

targets.script(
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

targets.compile_target(
    name = "chrome/updater:all",
    label = "//chrome/updater:all",
)

targets.console_test_launcher(
    name = "ui_unittests",
    label = "//ui/tests:ui_unittests",
)

targets.generated_script(
    name = "upload_trace_processor",
    label = "//tools/perf/core/perfetto_binary_roller:upload_trace_processor",
)

targets.console_test_launcher(
    name = "url_unittests",
    label = "//url:url_unittests",
)

targets.console_test_launcher(
    name = "usage_time_limit_unittests",
    label = "//chrome/test:usage_time_limit_unittests",
)

targets.compile_target(
    name = "v4l2_stateless_decoder",
    label = "//media/gpu/v4l2:v4l2_stateless_decoder",
)

targets.compile_target(
    name = "v4l2_unittest",
    label = "//media/gpu/v4l2:v4l2_unittest",
)

targets.console_test_launcher(
    name = "vaapi_unittest",
    label = "//media/gpu/vaapi:vaapi_unittest",
)

targets.console_test_launcher(
    name = "fake_libva_driver_unittest",
    label = "//media/gpu/vaapi/test/fake_libva_driver:fake_libva_driver_unittest",
)

targets.compile_target(
    name = "video_decode_accelerator_tests",
    label = "//media/gpu/test:video_decode_accelerator_tests",
)

targets.compile_target(
    name = "video_decode_accelerator_perf_tests",
    label = "//media/gpu/test:video_decode_accelerator_perf_tests",
)

targets.compile_target(
    name = "video_encode_accelerator_tests",
    label = "//media/gpu/test:video_encode_accelerator_tests",
)

targets.compile_target(
    name = "video_encode_accelerator_perf_tests",
    label = "//media/gpu/test:video_encode_accelerator_perf_tests",
)

targets.windowed_test_launcher(
    name = "views_examples_unittests",
    label = "//ui/views/examples:views_examples_unittests",
)

targets.script(
    name = "views_perftests",
    label = "//ui/views:views_perftests",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "--xvfb",
        "--non-telemetry=true",
        "views_perftests",
    ],
)

targets.windowed_test_launcher(
    name = "views_unittests",
    label = "//ui/views:views_unittests",
)

targets.windowed_test_launcher(
    name = "viz_unittests",
    label = "//components/viz:viz_unittests",
)

targets.console_test_launcher(
    name = "vr_android_unittests",
    label = "//chrome/browser/android/vr:vr_android_unittests",
)

targets.script(
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

targets.console_test_launcher(
    name = "vr_common_unittests",
    label = "//chrome/browser/vr:vr_common_unittests",
)

targets.script(
    name = "vr_perf_tests",
    label = "//tools/perf/contrib/vr_benchmarks:vr_perf_tests",
    script = "//testing/scripts/run_performance_tests.py",
    args = [
        "../../tools/perf/run_benchmark",
    ],
)

targets.script(
    name = "vrcore_fps_test",
    label = "//chrome/test/vr/perf:vrcore_fps_test",
    script = "//chrome/test/vr/perf/vrcore_fps/run_vrcore_fps_test.py",
    args = [
        "-v",
    ],
)

targets.windowed_test_launcher(
    name = "vulkan_tests",
    label = "//gpu/vulkan:vulkan_tests",
)

targets.windowed_test_launcher(
    name = "wayland_client_perftests",
    label = "//components/exo/wayland:wayland_client_perftests",
)

targets.windowed_test_launcher(
    name = "wayland_client_tests",
    label = "//components/exo/wayland:wayland_client_tests",
)

targets.console_test_launcher(
    name = "web_engine_browsertests",
    label = "//fuchsia_web/webengine:web_engine_browsertests",
)

targets.console_test_launcher(
    name = "web_engine_integration_tests",
    label = "//fuchsia_web/webengine:web_engine_integration_tests",
)

targets.compile_target(
    name = "web_engine_shell_pkg",
    label = "//fuchsia_web/shell:web_engine_shell_pkg",
)

targets.console_test_launcher(
    name = "web_engine_unittests",
    label = "//fuchsia_web/webengine:web_engine_unittests",
)

targets.generated_script(
    name = "webapk_client_junit_tests",
    label = "//chrome/android/webapk/libs/client:webapk_client_junit_tests",
)

targets.generated_script(
    name = "webapk_shell_apk_h2o_junit_tests",
    label = "//chrome/android/webapk/shell_apk:webapk_shell_apk_h2o_junit_tests",
)

targets.generated_script(
    name = "webapk_shell_apk_junit_tests",
    label = "//chrome/android/webapk/shell_apk:webapk_shell_apk_junit_tests",
)

targets.generated_script(
    name = "webgpu_blink_web_tests",
    label = "//:webgpu_blink_web_tests",
    args = [
        "--results-directory",
        "${ISOLATED_OUTDIR}",
    ],
)

targets.script(
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
    ],
)

targets.script(
    name = "webview_trichrome_cts_tests",
    label = "//android_webview/test:webview_trichrome_cts_tests",
    script = "//android_webview/tools/run_cts.py",
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
    ],
)

targets.script(
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
    ],
)

targets.script(
    name = "webview_64_cts_tests",
    label = "//android_webview/test:webview_64_cts_tests",
    script = "//android_webview/tools/run_cts.py",
    args = [
        "--skip-expected-failures",
        "--use-webview-provider",
        "apks/SystemWebView64.apk",
        "--apk-under-test",
        "apks/SystemWebView64.apk",
        "--use-apk-under-test-flags-file",
        "-v",
    ],
)

targets.console_test_launcher(
    name = "webview_instrumentation_test_apk",
    label = "//android_webview/test:webview_instrumentation_test_apk",
)

targets.console_test_launcher(
    name = "webview_ui_test_app_test_apk",
    label = "//android_webview/tools/automated_ui_tests:webview_ui_test_app_test_apk",
    args = [
        "--use-apk-under-test-flags-file",
    ],
)

targets.windowed_test_launcher(
    name = "wm_unittests",
    label = "//ui/wm:wm_unittests",
)

targets.console_test_launcher(
    name = "wtf_unittests",
    label = "//third_party/blink/renderer/platform/wtf:wtf_unittests",
)

targets.windowed_test_launcher(
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
    ],
)

targets.generated_script(
    name = "xvfb_py_unittests",
    label = "//testing:xvfb_py_unittests",
)

targets.console_test_launcher(
    name = "zlib_unittests",
    label = "//third_party/zlib:zlib_unittests",
    args = [
        "--test-launcher-timeout=400000",
    ],
)

targets.console_test_launcher(
    name = "zucchini_unittests",
    label = "//components/zucchini:zucchini_unittests",
)
