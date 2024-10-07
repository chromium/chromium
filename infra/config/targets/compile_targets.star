# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/targets.star", "targets")

targets.compile_target(
    name = "all",
)

targets.compile_target(
    name = "android_lint",
    label = "//chrome/android:android_lint",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "android_tools",
    label = "//tools/android:android_tools",
)

targets.compile_target(
    name = "base_nocompile_tests",
    label = "//base:base_nocompile_tests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "blink_platform_nocompile_tests",
    label = "//third_party/blink/renderer/platform:blink_platform_nocompile_tests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "blink_probes_nocompile_tests",
    label = "//third_party/blink/renderer/core/probe:blink_probes_nocompile_tests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "blink_tests",
    label = "//:blink_tests",
)

targets.compile_target(
    name = "cast_runner_pkg",
    label = "//fuchsia_web/runners:cast_runner_pkg",
)

targets.compile_target(
    name = "cast_junit_test_lists",
    label = "//chromecast:cast_junit_test_lists",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "cast_shell",
    label = "//chromecast:cast_shell",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "cast_browser_apk",
    label = "//chromecast:cast_browser_apk",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "cast_test_lists",
    label = "//chromecast:cast_test_lists",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "chrome",
    label = "//chrome:chrome",
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
    name = "chrome_public_apk",
    label = "//chrome/android:chrome_public_apk",
)

targets.compile_target(
    name = "chrome_sandbox",
    label = "//sandbox/linux:chrome_sandbox",
)

targets.compile_target(
    name = "chromedriver",
    label = "//chrome/test/chromedriver:chromedriver_server",
)

targets.compile_target(
    name = "chromedriver_group",
    label = "//:chromedriver_group",
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

targets.compile_target(
    name = "content_nocompile_tests",
    label = "//content/test:content_nocompile_tests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "core_runtime_simple",
    label = "//chromecast/cast_core:core_runtime_simple",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "core_runtime_starboard",
    label = "//chromecast/cast_core:core_runtime_starboard",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "cronet_package",
    label = "//components/cronet:cronet_package",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "cronet_package_ci",
    label = "//components/cronet/android:cronet_package_ci",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "cronet_perf_test_apk",
    label = "//components/cronet/android:cronet_perf_test_apk",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "empty_main",
    label = "//testing:empty_main",
)

targets.compile_target(
    name = "gn_all",
    label = "//:gn_all",
)

targets.compile_target(
    name = "image_processor_perf_test",
    label = "//media/gpu/chromeos:image_processor_perf_test",
)

targets.compile_target(
    name = "ios/chrome/app:chrome",
    label = "//ios/chrome/app:chrome",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "ios/chrome/test:all_fuzzer_tests",
    label = "//ios/chrome/test:all_fuzzer_tests",
    # All references have been moved to starlark
    skip_usage_check = True,
)

targets.compile_target(
    name = "ios_chrome_clusterfuzz_asan_build",
    label = "//ios/chrome/test/wpt:ios_chrome_clusterfuzz_asan_build",
)

targets.compile_target(
    name = "linux_symbols",
    label = "//chrome:linux_symbols",
)

targets.compile_target(
    name = "mini_installer",
    label = "//chrome/installer/mini_installer:mini_installer",
)

targets.compile_target(
    name = "mojo_rust",
    # Since we can't build rust tests on Android now, add this for build
    # coverage.
    label = "//mojo/public/rust:mojo_rust",
)

targets.compile_target(
    name = "check_chrome_static_initializers",
    label = "//chrome/android:check_chrome_static_initializers",
    # All references have been moved to starlark
    skip_usage_check = True,
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

targets.compile_target(
    name = "ondevice_model_benchmark",
    label = "//components/optimization_guide/internal:ondevice_model_benchmark",
)

targets.compile_target(
    name = "ondevice_model_example",
    label = "//components/optimization_guide/internal:ondevice_model_example",
)

targets.compile_target(
    name = "pdf_fuzzers",
    label = "//pdf/pdfium/fuzzers:pdf_fuzzers",
)

targets.compile_target(
    name = "pdfium_test",
    label = "//third_party/pdfium/testing:pdfium_test",
)

targets.compile_target(
    name = "postmortem-metadata",
    label = "//v8:postmortem-metadata",
)

targets.compile_target(
    name = "previous_version_mini_installer",
    label = "//chrome/installer/mini_installer:previous_version_mini_installer",
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

targets.compile_target(
    name = "rust_build_tests",
    label = "//build/rust/tests",
)

targets.compile_target(
    name = "symupload",
    label = "//third_party/breakpad:symupload",
)

targets.compile_target(
    name = "system_webview_apk",
    label = "//android_webview:system_webview_apk",
)

targets.compile_target(
    name = "system_webview_shell_apk",
    label = "//android_webview/tools/system_webview_shell:system_webview_shell_apk",
)

targets.compile_target(
    name = "telemetry_gpu_integration_test_scripts_only",
    label = "//chrome/test:telemetry_gpu_integration_test_scripts_only",
)

targets.compile_target(
    name = "trace_processor_shell",
    label = "//third_party/perfetto/src/trace_processor:trace_processor_shell",
)

targets.compile_target(
    name = "traffic_annotation_auditor_dependencies",
    label = "//:traffic_annotation_auditor_dependencies",
)

targets.compile_target(
    name = "traffic_annotation_proto",
    label = "//chrome/browser/privacy:traffic_annotation_proto",
)

targets.compile_target(
    name = "chrome/updater:all",
    label = "//chrome/updater:all",
)

targets.compile_target(
    name = "v4l2_stateless_decoder",
    label = "//media/gpu/v4l2:v4l2_stateless_decoder",
)

targets.compile_target(
    name = "v4l2_unittest",
    label = "//media/gpu/v4l2:v4l2_unittest",
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

targets.compile_target(
    name = "web_engine_shell_pkg",
    label = "//fuchsia_web/shell:web_engine_shell_pkg",
)

targets.compile_target(
    name = "chrome/enterprise_companion:all",
    label = "//chrome/enterprise_companion:all",
)
