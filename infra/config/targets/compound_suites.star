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
    name = "devtools_gtests",
    basic_suites = [
        "devtools_browser_tests_suite",
        "blink_unittests_suite",
    ],
)

# BEGIN composition test suites used by the GPU bots

targets.legacy_compound_suite(
    name = "gpu_chromeos_telemetry_tests",
    basic_suites = [
        "gpu_webgl_conformance_telemetry_tests",
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
    name = "win_specific_isolated_scripts_and_sizes",
    basic_suites = [
        "chrome_sizes_suite",
        "win_specific_isolated_scripts",
    ],
)
