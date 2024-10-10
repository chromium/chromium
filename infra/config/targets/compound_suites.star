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

targets.legacy_compound_suite(
    name = "android_cronet_clang_coverage_gtests",
    basic_suites = [
        "cronet_clang_coverage_additional_gtests",
        "cronet_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "android_oreo_emulator_gtests",
    basic_suites = [
        "android_emulator_specific_chrome_public_tests",
        "android_emulator_specific_network_enabled_content_browsertests",
        "android_monochrome_smoke_tests",
        "android_smoke_tests",
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "android_wpr_record_replay_tests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "linux_flavor_specific_chromium_gtests",
        "system_webview_shell_instrumentation_tests",  # Not an experimental test
        "webview_cts_tests_gtest",
        "webview_ui_instrumentation_tests",
        "webview_instrumentation_test_apk_single_process_mode_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "android_pie_coverage_instrumentation_tests",
    basic_suites = [
        "android_smoke_tests",
        "android_specific_coverage_java_tests",
        "chrome_public_tests",
        "vr_android_specific_chromium_tests",
        "webview_ui_instrumentation_tests",
    ],
)

targets.legacy_compound_suite(
    name = "bfcache_linux_gtests",
    basic_suites = [
        "bfcache_generic_gtests",
        "bfcache_linux_specific_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "chrome_linux_isolated_script_tests",
    basic_suites = [
        "chrome_isolated_script_tests",
        "chrome_private_code_test_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "chromeos_vm_gtests",
    basic_suites = [
        "chromeos_system_friendly_gtests",
        "chromeos_vaapi_fakelib_gtests",
        "chromeos_integration_tests_suite",
    ],
)

targets.legacy_compound_suite(
    name = "chromeos_vm_tast",
    basic_suites = [
        "chromeos_browser_all_tast_tests",
        "chromeos_browser_criticalstaging_tast_tests",
        "chromeos_browser_disabled_tast_tests",
        "chromeos_browser_integration_tests",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_android_gtests",
    basic_suites = [
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

# This is meant to be a superset of 'chromium_linux_and_gl_gtests'. Any
# changes there must be reflected here.
targets.legacy_compound_suite(
    name = "chromium_linux_and_gl_and_vulkan_gtests",
    basic_suites = [
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_chromeos_only",
        "chromium_gtests_for_linux_and_mac_only",
        "chromium_gtests_for_linux_only",
        "chromium_gtests_for_win_and_linux_only",
        "linux_flavor_specific_chromium_gtests",
        "linux_specific_xr_gtests",
        "gl_gtests_passthrough",
        "gpu_fyi_vulkan_swiftshader_gtests",
        "non_android_and_cast_and_chromeos_chromium_gtests",
        "non_android_chromium_gtests_no_nacl",
        "vr_platform_specific_chromium_gtests",
    ],
)

# gl_tests requires dedicated machines with GPUs on linux, so have a separate
# test list with gl_tests included. This is chromium_linux_gtests + gl_gtests.
targets.legacy_compound_suite(
    name = "chromium_linux_and_gl_gtests",
    basic_suites = [
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_chromeos_only",
        "chromium_gtests_for_linux_and_mac_only",
        "chromium_gtests_for_linux_only",
        "chromium_gtests_for_win_and_linux_only",
        "linux_flavor_specific_chromium_gtests",
        "linux_specific_xr_gtests",
        "gl_gtests_passthrough",
        "non_android_and_cast_and_chromeos_chromium_gtests",
        "non_android_chromium_gtests_no_nacl",
        "vr_platform_specific_chromium_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_linux_blink_rel_isolated_scripts",
    basic_suites = [
        "chromium_webkit_isolated_scripts",
        "linux_specific_chromium_isolated_scripts",
        "vulkan_swiftshader_isolated_scripts",
        "chromium_web_tests_high_dpi_isolated_scripts",
    ],
)

# When changing something here, change chromium_linux_and_gl_gtests,
# chromium_linux_and_gl_and_vulkan_gtests in the same way.
targets.legacy_compound_suite(
    name = "chromium_linux_gtests",
    basic_suites = [
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

targets.legacy_compound_suite(
    name = "chromium_linux_rel_isolated_scripts",
    basic_suites = [
        "chromedriver_py_tests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "linux_specific_chromium_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "pytype_tests",
        "telemetry_perf_unittests_isolated_scripts",
        "vulkan_swiftshader_isolated_scripts",
        "chromium_web_tests_high_dpi_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_linux_rel_isolated_scripts_code_coverage",
    basic_suites = [
        "chromedriver_py_tests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "linux_specific_chromium_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "pytype_tests",
        "telemetry_perf_unittests_isolated_scripts_xvfb",
        "vulkan_swiftshader_isolated_scripts",
        "chromium_web_tests_high_dpi_isolated_scripts",
        "gpu_dawn_webgpu_blink_web_tests",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_mac_gtests",
    basic_suites = [
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_mac_only",
        "mac_specific_chromium_gtests",
        "non_android_and_cast_and_chromeos_chromium_gtests",
        "non_android_chromium_gtests_no_nacl",
    ],
)

# chromium_mac_gtests_no_nacl_once in the same way.
# TODO(b/303417958): This no_nacl suite is identical to the normal suite, since
# NaCl has been disabled on Mac. Replace this by the normal suite.
targets.legacy_compound_suite(
    name = "chromium_mac_gtests_no_nacl",
    basic_suites = [
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_mac_only",
        "mac_specific_chromium_gtests",
        "non_android_and_cast_and_chromeos_chromium_gtests",
        "non_android_chromium_gtests_no_nacl",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_mac_osxbeta_rel_isolated_scripts",
    basic_suites = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_mac_osxbeta_scripts",
        "mac_specific_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_mac_rel_isolated_scripts_and_sizes",
    basic_suites = [
        "chrome_sizes_suite",
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "mac_specific_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_mac_rel_isolated_scripts_code_coverage",
    basic_suites = [
        # TODO(crbug.com/40249801): Enable gpu_dawn_webgpu_blink_web_tests
    ],
)

# Like chromium_mac_rel_isolated_scripts above, but should only
# include test suites that aren't affected by things like extra GN args
# (e.g. is_debug) or OS versions (e.g. Mac-12 vs Mac-13). Note: use
# chromium_mac_rel_isolated_scripts if you're setting up a new builder.
# This group should only be used across ~3 builders.
targets.legacy_compound_suite(
    name = "chromium_mac_rel_isolated_scripts_once",
    basic_suites = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "desktop_once_isolated_scripts",
        "mac_specific_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

# Multiscreen tests for desktop platforms. See: crbug.com/346565331.
targets.legacy_compound_suite(
    name = "chromium_multiscreen_gtests_fyi",
    basic_suites = [
        "chromium_multiscreen_gtests",
    ],
)

# Pixel tests only enabled on Win 10. So this is
# 'chromium_win_gtests' + 'pixel_browser_tests_gtests' +
# 'non_android_chromium_gtests_skia_gold'. When changing
# something here, also change chromium_win10_gtests_once in the same way.
targets.legacy_compound_suite(
    name = "chromium_win10_gtests",
    basic_suites = [
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

targets.legacy_compound_suite(
    name = "chromium_win_gtests",
    basic_suites = [
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

targets.legacy_compound_suite(
    name = "chromium_win_rel_isolated_scripts",
    basic_suites = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "telemetry_desktop_minidump_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
        "win_specific_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_win_rel_isolated_scripts_code_coverage",
    basic_suites = [
        "gpu_dawn_webgpu_blink_web_tests",
    ],
)

targets.legacy_compound_suite(
    name = "devtools_gtests",
    basic_suites = [
        "devtools_browser_tests_suite",
        "blink_unittests_suite",
    ],
)

# All gtests that can be run on Fuchsia CI/CQ
targets.legacy_compound_suite(
    name = "fuchsia_gtests",
    basic_suites = [
        "fuchsia_chrome_gtests",
        "fuchsia_web_engine_gtests",
    ],
)

# BEGIN composition test suites used by the GPU bots

targets.legacy_compound_suite(
    name = "gpu_angle_linux_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_angle_mac_telemetry_tests",
    basic_suites = [
        "gpu_info_collection_telemetry_tests",
        "gpu_webgl2_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl2_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_angle_win_intel_nvidia_telemetry_tests",
    basic_suites = [
        "gpu_info_collection_telemetry_tests",
        "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_chromeos_telemetry_tests",
    basic_suites = [
        "gpu_webgl_conformance_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_common_android_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_validating_telemetry_tests",
        "gpu_webgl_conformance_validating_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_common_gl_passthrough_ganesh_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests",
    ],
)

# Same as gpu_dawn_isolated_scripts, but with some suites removed:
# * telemetry_gpu_unittests since those aren't built for Android
# * SwiftShader-related tests since SwiftShader is not used on Android.
targets.legacy_compound_suite(
    name = "gpu_dawn_android_isolated_scripts",
    basic_suites = [
        "gpu_dawn_perf_smoke_isolated_scripts",
        "gpu_dawn_webgpu_blink_web_tests",
    ],
)

# Same as gpu_dawn_compat_telemetry_tests, but without SwiftShader tests since
# SwiftShader is not used on Android.
targets.legacy_compound_suite(
    name = "gpu_dawn_android_compat_telemetry_tests",
    basic_suites = [
        "gpu_dawn_webgpu_compat_cts",
        "gpu_dawn_webgpu_cts",
    ],
)

# Same as gpu_dawn_telemetry_tests, but without SwiftShader tests since
# SwiftShader is not used on Android.
targets.legacy_compound_suite(
    name = "gpu_dawn_android_telemetry_tests",
    basic_suites = [
        "gpu_dawn_webgpu_cts",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_dawn_asan_isolated_scripts",
    basic_suites = [
        "gpu_dawn_common_isolated_scripts",
        "gpu_dawn_perf_smoke_isolated_scripts",
        "gpu_dawn_webgpu_blink_web_tests",
        "gpu_dawn_webgpu_blink_web_tests_force_swiftshader",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_dawn_compat_telemetry_tests",
    basic_suites = [
        "gpu_dawn_webgpu_compat_cts",
        "gpu_dawn_webgpu_cts",
        "gpu_dawn_web_platform_webgpu_cts_force_swiftshader",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_dawn_integration_asan_gtests_passthrough",
    basic_suites = [
        "gpu_dawn_gtests",
        "gpu_dawn_gtests_no_dxc",
        "gpu_common_gtests_passthrough",
    ],
)

# GPU gtests that test Dawn and integration with Chromium
# These tests are run both on the CI and trybots which test DEPS Dawn.
targets.legacy_compound_suite(
    name = "gpu_dawn_integration_gtests_passthrough",
    basic_suites = [
        "gpu_dawn_gtests",
        "gpu_dawn_gtests_with_validation",
        "gpu_common_gtests_passthrough",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_dawn_integration_gtests_passthrough_win_x64",
    basic_suites = [
        "gpu_dawn_gtests",
        "gpu_dawn_gtests_with_validation",
        "gpu_dawn_gtests_no_dxc",
        "gpu_dawn_gtests_no_dxc_with_validation",
        "gpu_common_gtests_passthrough",
    ],
)

# TODO(crbug.com/364675466): Remove this when Tint IR is launched on macOS.
targets.legacy_compound_suite(
    name = "gpu_dawn_integration_gtests_passthrough_macos",
    basic_suites = [
        "gpu_dawn_gtests",
        "gpu_dawn_gtests_with_validation",
        "gpu_dawn_gtests_use_tint_ir",
        "gpu_common_gtests_passthrough",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_dawn_isolated_scripts",
    basic_suites = [
        "gpu_dawn_common_isolated_scripts",
        "gpu_dawn_perf_smoke_isolated_scripts",
        "gpu_dawn_webgpu_blink_web_tests",
        "gpu_dawn_webgpu_blink_web_tests_force_swiftshader",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_dawn_telemetry_tests",
    basic_suites = [
        "gpu_dawn_webgpu_cts",
        "gpu_dawn_web_platform_webgpu_cts_force_swiftshader",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_dawn_telemetry_tests_fxc",
    basic_suites = [
        "gpu_dawn_webgpu_cts_fxc",
        "gpu_dawn_web_platform_webgpu_cts_force_swiftshader",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_dawn_telemetry_win_x64_tests",
    basic_suites = [
        "gpu_dawn_webgpu_cts",
        "gpu_dawn_webgpu_cts_fxc",
        "gpu_dawn_web_platform_webgpu_cts_force_swiftshader",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_dawn_tsan_gtests",
    basic_suites = [
        "gpu_dawn_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_android_gtests",
    basic_suites = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_common_gtests_validating",
        "gpu_fyi_and_optional_non_linux_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_android_shieldtv_gtests",
    basic_suites = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_common_gtests_validating",
        "gpu_fyi_and_optional_non_linux_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_android_webgl2_and_gold_telemetry_tests",
    basic_suites = [
        "gpu_validating_telemetry_tests",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_validating_telemetry_tests",
    ],
)

# TODO(crbug.com/40130073): Merge with an existing set of tests such as
# gpu_fyi_linux_release_gtests once all CrOS tests have been enabled.
targets.legacy_compound_suite(
    name = "gpu_fyi_chromeos_release_gtests",
    basic_suites = [
        # TODO(crbug.com/40723796): Missing cros wrapper script.
        # "gpu_angle_unit_gtests",
        # TODO(crbug.com/1087567, crbug.com/1087590): Enable once there are tests
        # that actually pass.
        "gpu_common_gtests_passthrough",
        # TODO(crbug.com/1087563): Enable once tab_capture_end2end_tests passes
        # on CrOS.
        # "gpu_desktop_specific_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_chromeos_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
    ],
)

# The same as gpu_fyi_chromeos_release_telemetry_tests, but using
# passthrough instead of validating since the Lacros bots are actually
# Lacros-like Linux bots, and Linux uses the passthrough decoder.
# Additionally, we use GLES instead of GL since that's what is supported.
targets.legacy_compound_suite(
    name = "gpu_fyi_lacros_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_lacros_release_gtests",
    basic_suites = [
        "gpu_memory_buffer_impl_tests_suite",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_linux_debug_gtests",
    basic_suites = [
        "gpu_common_gtests_passthrough",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_linux_debug_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_linux_release_gtests",
    basic_suites = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_desktop_specific_gtests",
        "gpu_memory_buffer_impl_tests_suite",
        "gpu_vulkan_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_linux_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
    ],
)

# TODO(jonross): remove this once Vulkan Swiftshader and Vulkan GL interop
# paths are merged. This should mirror
# `gpu_fyi_linux_release_telemetry_tests` but with additional
# `gpu_skia_renderer_vulkan_passthrough_telemetry_tests`
targets.legacy_compound_suite(
    name = "gpu_fyi_linux_release_vulkan_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
        "gpu_skia_renderer_vulkan_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_mac_debug_gtests",
    basic_suites = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_fyi_and_optional_non_linux_gtests",
        "gpu_fyi_mac_specific_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_mac_nvidia_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webcodecs_gl_passthrough_ganesh_telemetry_test",
        "gpu_webgl2_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_mac_pro_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl2_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_graphite_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_mac_release_gtests",
    basic_suites = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_desktop_specific_gtests",
        "gpu_fyi_and_optional_non_linux_gtests",
        "gpu_fyi_mac_specific_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_mac_release_telemetry_tests",
    basic_suites = [
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

targets.legacy_compound_suite(
    name = "gpu_fyi_only_mac_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_gl_passthrough_ganesh_telemetry_tests",
        "gpu_metal_passthrough_ganesh_telemetry_tests",
        "gpu_metal_passthrough_graphite_telemetry_tests",
        "gpu_webcodecs_gl_passthrough_ganesh_telemetry_test",
        "gpu_webcodecs_metal_passthrough_ganesh_telemetry_test",
        "gpu_webcodecs_metal_passthrough_graphite_telemetry_test",
        "gpu_webgl2_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl2_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_win_amd_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_win_debug_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
        "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_win_gtests",
    basic_suites = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_default_and_optional_win_media_foundation_specific_gtests",
        "gpu_default_and_optional_win_specific_gtests",
        "gpu_desktop_specific_gtests",
        "gpu_fyi_and_optional_non_linux_gtests",
        "gpu_fyi_and_optional_win_specific_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_win_intel_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
        "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_win_optional_isolated_scripts",
    basic_suites = [
        "gpu_command_buffer_perf_passthrough_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_win_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_graphite_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
        "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_nexus5x_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_validating_telemetry_tests",
        "gpu_webcodecs_validating_telemetry_test",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_validating_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_nexus5x_telemetry_tests_v8",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_validating_telemetry_tests",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_validating_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_pixel_4_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_validating_telemetry_tests",
        "gpu_webcodecs_validating_telemetry_test",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_validating_telemetry_tests",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_validating_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_pixel_6_telemetry_tests",
    basic_suites = [
        "gpu_passthrough_graphite_telemetry_tests",
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_validating_telemetry_tests",
        "gpu_webcodecs_validating_telemetry_test",
        "gpu_webcodecs_validating_graphite_telemetry_test",
        "gpu_webgl_conformance_gles_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_validating_telemetry_tests",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_validating_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_swangle_telemetry_tests",
    basic_suites = [
        "gpu_webgl_conformance_swangle_passthrough_telemetry_tests",
    ],
)

# END composition test suites used by the GPU bots

# This is:
#   linux_chromium_gtests
#   - non_android_and_cast_and_chromeos_chromium_gtests
#   + linux_chromeos_lacros_gtests
#   + linux_chromeos_specific_gtests
targets.legacy_compound_suite(
    name = "linux_chromeos_gtests",
    basic_suites = [
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_chromeos_only",
        "chromium_gtests_for_win_and_linux_only",
        "linux_chromeos_lacros_gtests",
        "linux_chromeos_specific_gtests",
        "linux_flavor_specific_chromium_gtests",
        "non_android_chromium_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "linux_chromeos_gtests_oobe",
    basic_suites = [
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

targets.legacy_compound_suite(
    name = "linux_chromeos_isolated_scripts",
    basic_suites = [
        "blink_web_tests_ppapi_isolated_scripts",
        "chrome_sizes_suite",
    ],
)

# This is for linux-chromeos-rel CQ builder.
targets.legacy_compound_suite(
    name = "linux_chromeos_rel_cq",
    basic_suites = [
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
        "ash_pixel_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "linux_viz_gtests",
    basic_suites = [
        "gpu_fyi_vulkan_swiftshader_gtests",
    ],
)

# TODO(crbug.com/40142574): Re-enable this if/when additional capacity
# targets.legacy_compound_suite(
#     name = 'marshmallow_nougat_pie_isolated_scripts_with_proguard',
#     basic_suites = [
#         'android_isolated_scripts',
#         'components_perftests_isolated_scripts',
#         'telemetry_android_minidump_unittests_isolated_scripts',
#         'telemetry_perf_unittests_isolated_scripts_android',
#     ],
# )

targets.legacy_compound_suite(
    name = "network_service_extra_gtests",
    basic_suites = [
        "network_service_fyi_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "oreo_isolated_scripts",
    basic_suites = [
        "android_isolated_scripts",
        "chromium_junit_tests_scripts",
        "components_perftests_isolated_scripts",
        "monochrome_public_apk_checker_isolated_script",
        "telemetry_android_minidump_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts_android",
    ],
)

# Rust tests run on non-cross builds.
targets.legacy_compound_suite(
    name = "rust_host_gtests",
    basic_suites = [
        "rust_common_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "webview_fyi_bot_all_gtests",
    basic_suites = [
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

targets.legacy_compound_suite(
    name = "webview_native_coverage_bot_gtests",
    basic_suites = [
        "webview_bot_instrumentation_test_apk_mutations_gtest",
        "webview_bot_instrumentation_test_apk_no_field_trial_gtest",
        "webview_bot_unittests_gtest",
    ],
)

targets.legacy_compound_suite(
    name = "win_specific_isolated_scripts_and_sizes",
    basic_suites = [
        "chrome_sizes_suite",
        "win_specific_isolated_scripts",
    ],
)
