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
    name = "android_10_rel_gtests",
    basic_suites = [
        "android_trichrome_smoke_tests",
        "android_ar_gtests",
        "android_ddready_vr_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "android_12_dbg_emulator_gtests",
    basic_suites = [
        "android_trichrome_smoke_tests",
    ],
)

targets.legacy_compound_suite(
    name = "android_marshmallow_gtests",
    basic_suites = [
        "android_smoke_tests",
        "android_specific_chromium_gtests",  # Already includes gl_gtests.
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chrome_public_tests",
        "linux_flavor_specific_chromium_gtests",
        "vr_android_specific_chromium_tests",
        "vr_platform_specific_chromium_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "android_oreo_emulator_gtests",
    basic_suites = [
        "android_emulator_specific_chrome_public_tests",
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
    ],
)

targets.legacy_compound_suite(
    name = "android_oreo_gtests",
    basic_suites = [
        "android_ar_gtests",
        "android_ddready_vr_gtests",
        "android_monochrome_smoke_tests",
        "android_oreo_standard_gtests",
        "android_smoke_tests",
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
    name = "android_pie_gtests",
    basic_suites = [
        "android_ar_gtests",
        "android_ddready_vr_gtests",
        "android_monochrome_smoke_tests",
        "android_smoke_tests",
        "chromium_tracing_gtests",
        # No standard tests due to capacity, no Vega tests since it's currently
        # O only.
    ],
)

# Keep in sync with android_pie_rel_gtests below, except for
# vr_{android,platform}_specific_chromium_gtests which are not applicable
# to android emulators on x86 & x64.
targets.legacy_compound_suite(
    name = "android_pie_rel_emulator_gtests",
    basic_suites = [
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
    ],
)

targets.legacy_compound_suite(
    name = "android_pie_rel_gtests",
    basic_suites = [
        # TODO(crbug.com/1111436): Deprecate this when all the test suites below
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
        "webview_64_cts_tests_gtest",
        "webview_ui_instrumentation_tests",
    ],
)

targets.legacy_compound_suite(
    name = "bfcache_android_gtests",
    basic_suites = [
        "bfcache_android_specific_gtests",
        "bfcache_generic_gtests",
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
    name = "chromeos_device_gtests",
    basic_suites = [
        "chromeos_browser_all_tast_tests",
        "chromeos_browser_integration_tests",
        "chromeos_device_only_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "chromeos_device_no_gtests",
    basic_suites = [
        "chromeos_browser_all_tast_tests",
        "chromeos_browser_integration_tests",
    ],
)

targets.legacy_compound_suite(
    name = "chromeos_vm_gtests",
    basic_suites = [
        "chromeos_system_friendly_gtests",
        "chromeos_integration_tests",
    ],
)

targets.legacy_compound_suite(
    name = "chromeos_vm_gtests_and_tast",
    basic_suites = [
        "chromeos_browser_all_tast_tests",
        "chromeos_browser_integration_tests",
        "chromeos_system_friendly_gtests",
        "chromeos_integration_tests",
    ],
)

targets.legacy_compound_suite(
    name = "chromeos_vm_tast",
    basic_suites = [
        "chromeos_browser_all_tast_tests",
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
    ],
)

targets.legacy_compound_suite(
    name = "chromium_dbg_isolated_scripts",
    basic_suites = [
        "desktop_chromium_isolated_scripts",
        "performance_smoke_test_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
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

targets.legacy_compound_suite(
    name = "chromium_linux_cast_audio_gtests",
    basic_suites = [
        "cast_audio_specific_chromium_gtests",
        "chromium_gtests",
        "linux_flavor_specific_chromium_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_linux_cast_video_gtests",
    basic_suites = [
        "cast_audio_specific_chromium_gtests",
        "cast_video_specific_chromium_gtests",
        "chromium_gtests",
        "linux_flavor_specific_chromium_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_linux_dbg_isolated_scripts",
    basic_suites = [
        "desktop_chromium_isolated_scripts",
        "linux_specific_chromium_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

# When changing something here, change chromium_linux_and_gl_gtests,
# chromium_linux_and_gl_and_vulkan_gtests, and
# chromium_linux_rel_gtests_once in the same way.
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

# TODO(crbug.com/1444855): This set should match chromium_linux_gtests,
# except that it also runs tests that we can afford to run only once on
# Linux machines (for now, this is just the cr23_linux_gtests).
#
# Delete this test suite after the ChromeRefresh2023 is fully rolled out
# (assuming no other test suites are being run only once) and make sure
# any bots go back to using chromium_linux_gtests.
targets.legacy_compound_suite(
    name = "chromium_linux_gtests_once",
    basic_suites = [
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_chromeos_only",
        "chromium_gtests_for_linux_and_mac_only",
        "chromium_gtests_for_linux_only",
        "chromium_gtests_for_win_and_linux_only",
        "cr23_linux_gtests",
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
        # TODO(weizhong): we should eventually run chrome_wpt_tests where
        # blink_wpt_tests runs on Linux. There should not have any resource
        # concern on this because those are all CI builders.
        #"chromium_wpt_tests_isolated_scripts",
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

# Like chromium_linux_rel_isolated_scripts above, but should only
# include test suites that aren't affected by things like extra GN args
# (e.g. is_debug) or OS versions (e.g. Mac-12 vs Mac-13). Note: use
# chromium_linux_rel_isolated_scripts if you're setting up a new builder.
# This group should only be used across ~3 builders.
targets.legacy_compound_suite(
    name = "chromium_linux_rel_isolated_scripts_once",
    basic_suites = [
        "chromedriver_py_tests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "desktop_once_isolated_scripts",
        "linux_specific_chromium_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "pytype_tests",
        "telemetry_perf_unittests_isolated_scripts",
        "vulkan_swiftshader_isolated_scripts",
        "chromium_web_tests_high_dpi_isolated_scripts",
        # TODO(crbug.com/1498364): Remove this once the BackgroundResourceFetch
        # feature launches.
        "chromium_web_tests_brfetch_isolated_scripts",
        "chromium_wpt_tests_isolated_scripts",
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

# When changing something here, change
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

# TODO(crbug.com/1444855): This set should match
# chromium_mac_gtests_no_nacl, except that it also runs tests that we can
# only afford to run once on Mac machines (for now, this is just the
# cr23_mac_gtests).
#
# Delete this test suite after the ChromeRefresh2023 is fully rolled out
# and make sure any bots go back to using
# chromium_mac_gtests_no_nacl_no_nocompile.
targets.legacy_compound_suite(
    name = "chromium_mac_gtests_no_nacl_once",
    basic_suites = [
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "cr23_mac_gtests",
        "mac_specific_chromium_gtests",
        "non_android_and_cast_and_chromeos_chromium_gtests",
        "non_android_chromium_gtests_no_nacl",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_mac_rel_isolated_scripts",
    basic_suites = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "mac_specific_isolated_scripts",
        "mojo_python_unittests_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_mac_rel_isolated_scripts_and_sizes",
    basic_suites = [
        "chrome_sizes",
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
        # TODO(crbug.com/1399354): Enable gpu_dawn_webgpu_blink_web_tests
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
        "cr23_pixel_browser_tests_gtests",
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
    name = "chromium_win10_gtests_multiscreen_fyi",
    basic_suites = [
        "chromium_gtests_for_windows_multiscreen",
    ],
)

# TODO(crbug.com/1444855): This set should match chromium_win10_gtests,
# except that it also runs tests that we can afford to run only once
# on Windows machines (for now this is just the cr23_win_gtests).
#
# Delete this test suite after the ChromeRefresh2023 is fully rolled out
# and make sure any bots go back to using chromium_win10_gtests.
targets.legacy_compound_suite(
    name = "chromium_win10_gtests_once",
    basic_suites = [
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_win_and_linux_only",
        "cr23_pixel_browser_tests_gtests",
        "cr23_win_gtests",
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
    name = "chromium_win_dbg_isolated_scripts",
    basic_suites = [
        "chromedriver_py_tests_isolated_scripts",
        "components_perftests_isolated_scripts",
        "desktop_chromium_isolated_scripts",
        "performance_smoke_test_isolated_scripts",
        "telemetry_perf_unittests_isolated_scripts",
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

# Like chromium_win_rel_isolated_scripts above, but should only
# include test suites that aren't affected by things like extra GN args
# (e.g. is_debug) or OS versions (e.g. Mac-12 vs Mac-13). Note: use
# chromium_win_rel_isolated_scripts if you're setting up a new builder.
# This group should only be used across ~3 builders.
targets.legacy_compound_suite(
    name = "chromium_win_rel_isolated_scripts_once",
    basic_suites = [
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

targets.legacy_compound_suite(
    name = "cronet_dbg_isolated_scripts",
    basic_suites = [
        "cronet_sizes",
    ],
)

targets.legacy_compound_suite(
    name = "cronet_rel_isolated_scripts",
    basic_suites = [
        "cronet_resource_sizes",
        "cronet_sizes",
    ],
)

targets.legacy_compound_suite(
    name = "devtools_gtests",
    basic_suites = [
        "devtools_browser_tests",
        "blink_unittests",
    ],
)

# Runs only the accessibility tests in CI/CQ to reduce accessibility
# failures that land.
targets.legacy_compound_suite(
    name = "fuchsia_accessibility_browsertests",
    basic_suites = [
        "fuchsia_accessibility_content_browsertests",
    ],
)

targets.legacy_compound_suite(
    name = "fuchsia_arm64_isolated_scripts",
    basic_suites = [
        "fuchsia_sizes_tests",
        "gpu_angle_fuchsia_unittests_isolated_scripts",
    ],
)

# All gtests that can be run on Fuchsia CI/CQ
targets.legacy_compound_suite(
    name = "fuchsia_gtests",
    basic_suites = [
        "fuchsia_chrome_small_gtests",
        "fuchsia_common_gtests",
        "fuchsia_common_gtests_with_graphical_output",
        "web_engine_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "fuchsia_isolated_scripts",
    basic_suites = [
        "chromium_webkit_isolated_scripts",
        "gpu_angle_fuchsia_unittests_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "fuchsia_web_engine_non_graphical_gtests",
    basic_suites = [
        "fuchsia_common_gtests",
        "web_engine_gtests",
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
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_metal_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_telemetry_tests",
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
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_common_linux_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl_conformance_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_common_metal_passthrough_ganesh_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_metal_passthrough_ganesh_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_common_win_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
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
    name = "gpu_dawn_telemetry_win_x64_tests",
    basic_suites = [
        "gpu_dawn_webgpu_cts",
        "gpu_dawn_webgpu_cts_dxc",
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
    name = "gpu_desktop_mac_gtests",
    basic_suites = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_desktop_specific_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_desktop_passthrough_gtests",
    basic_suites = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_desktop_specific_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fuchsia_telemetry_tests",
    basic_suites = [
        "gpu_validating_telemetry_tests",
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

# TODO(crbug.com/1080424): Merge with an existing set of tests such as
# gpu_fyi_linux_release_gtests once all CrOS tests have been enabled.
targets.legacy_compound_suite(
    name = "gpu_fyi_chromeos_release_gtests",
    basic_suites = [
        # TODO(crbug.com/1135720): Missing cros wrapper script.
        # "gpu_angle_unit_gtests",
        # TODO(crbug.com/1087567, crbug.com/1087590): Enable once there are tests
        # that actually pass.
        "gpu_common_gtests_validating",
        # TODO(crbug.com/1087563): Enable once tab_capture_end2end_tests passes
        # on CrOS.
        # "gpu_desktop_specific_gtests",
    ],
)

# TODO(crbug.com/1080424): Merge with an existing set of tests such as
# gpu_fyi_linux_release_telemetry_tests once all CrOS tests
# have been enabled.
targets.legacy_compound_suite(
    name = "gpu_fyi_chromeos_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_mediapipe_validating_telemetry_tests",
        "gpu_validating_telemetry_tests",
        "gpu_webcodecs_validating_telemetry_test",
        "gpu_webgl_conformance_validating_telemetry_tests",
        # Large amounts of WebGL/WebGL2 tests are failing due to issues that are
        # possibly related to other CrOS issues that are already reported.
        # TODO(crbug.com/1080424): Try enabling these again once some of the
        # existing CrOS WebGL issues are resolved.
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_validating_telemetry_tests",
        # "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
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
        "gpu_mediapipe_passthrough_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_linux_debug_gtests",
    basic_suites = [
        "gpu_common_gtests_passthrough",
        "gpu_gles2_conform_gtests",
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
        "gpu_gles2_conform_gtests",
        "gpu_vulkan_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_linux_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_mediapipe_passthrough_telemetry_tests",
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
        "gpu_mediapipe_passthrough_telemetry_tests",
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
        "gpu_gles2_conform_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_mac_nvidia_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_gl_passthrough_ganesh_telemetry_tests",
        "gpu_mediapipe_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_mac_pro_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl2_conformance_metal_passthrough_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_telemetry_tests",
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
        "gpu_gles2_conform_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_mac_release_telemetry_tests",
    basic_suites = [
        "gpu_gl_passthrough_ganesh_telemetry_tests",
        "gpu_mediapipe_passthrough_telemetry_tests",
        "gpu_metal_passthrough_graphite_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_metal_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_only_mac_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_gl_passthrough_ganesh_telemetry_tests",
        "gpu_mediapipe_passthrough_telemetry_tests",
        "gpu_metal_passthrough_ganesh_telemetry_tests",
        "gpu_metal_passthrough_graphite_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl2_conformance_metal_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_telemetry_tests",
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_win_amd_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_mediapipe_passthrough_telemetry_tests",
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
        "gpu_gles2_conform_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_win_intel_release_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_mediapipe_passthrough_telemetry_tests",
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
        "gpu_mediapipe_passthrough_telemetry_tests",
        "gpu_mediapipe_validating_telemetry_tests",
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
    name = "gpu_pixel_4_and_6_telemetry_tests",
    basic_suites = [
        "gpu_common_and_optional_telemetry_tests",
        "gpu_mediapipe_passthrough_telemetry_tests",
        "gpu_mediapipe_validating_telemetry_tests",
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
    name = "gpu_swangle_telemetry_tests",
    basic_suites = [
        "gpu_webgl_conformance_swangle_passthrough_telemetry_tests",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_win_gtests",
    basic_suites = [
        "gpu_angle_unit_gtests",
        "gpu_common_gtests_passthrough",
        "gpu_default_and_optional_win_specific_gtests",
        "gpu_desktop_specific_gtests",
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
        "chrome_sizes",
    ],
)

# This is:
#   linux_chromeos_gtests
#   + 'linux_chromeos_browser_tests_require_lacros'
targets.legacy_compound_suite(
    name = "linux_chromeos_specific_and_lacros_dependent_gtests",
    basic_suites = [
        "aura_gtests",
        "chromium_gtests",
        "chromium_gtests_for_devices_with_graphical_output",
        "chromium_gtests_for_linux_and_chromeos_only",
        "chromium_gtests_for_win_and_linux_only",
        "linux_chromeos_lacros_gtests",
        "linux_chromeos_specific_gtests",
        "linux_chromeos_browser_tests_require_lacros",
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

# TODO(crbug.com/1111436): Re-enable this if/when additional capacity
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
    name = "webrtc_chromium_tests_with_baremetal_tests",
    basic_suites = [
        "webrtc_chromium_baremetal_gtests",
        "webrtc_chromium_gtests",
    ],
)

targets.legacy_compound_suite(
    name = "webview_bot_all_gtests",
    basic_suites = [
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
    name = "webview_trichrome_64_cts_gtests",
    basic_suites = [
        "webview_trichrome_64_cts_tests",
        "webview_trichrome_64_cts_tests_no_field_trial",
    ],
)

targets.legacy_compound_suite(
    name = "win_specific_isolated_scripts_and_sizes",
    basic_suites = [
        "chrome_sizes",
        "win_specific_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "wpt_web_tests_content_shell_multiple_flags",
    basic_suites = [
        "wpt_web_tests_content_shell",
        "wpt_web_tests_not_site_per_process",
        "wpt_web_tests_highdpi",
    ],
)
