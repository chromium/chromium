# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compound suite declarations

Compound suites are a collection of basic suites that can be referenced by a
builder in //testing/buildbot/waterfalls.pyl. Suites also define a bundle
containing the same tests as the suite, so they can be used wherever a bundle is
expected.

The legacy_ prefix denotes the ability for compound suites to be referenced in
//testing/buildbot. Once a suite is no longer referenced via //testing/buildbot,
targets.bundle can be used for grouping tests in a more flexible manner (mixing
test types and/or compile targets and arbitrary nesting). Named bundles are
defined in ./bundles.star.
"""

load("@chromium-luci//targets.star", "targets")

targets.legacy_compound_suite(
    name = "chrome_linux_isolated_script_tests",
    basic_suites = [
        "chrome_isolated_script_tests",
        "chrome_private_code_test_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "chromium_linux_blink_rel_isolated_scripts",
    basic_suites = [
        "chromium_blink_isolated_scripts",
        "linux_specific_chromium_isolated_scripts",
        "vulkan_swiftshader_isolated_scripts",
        "chromium_web_tests_high_dpi_isolated_scripts",
    ],
)

targets.legacy_compound_suite(
    name = "devtools_gtests",
    basic_suites = [
        "devtools_browser_tests_suite",
    ],
)

# BEGIN composition test suites used by the GPU bots

# TODO(jonross): remove this once Vulkan Swiftshader and Vulkan GL interop
# paths are merged. This should mirror
# `gpu_all_linux_release_telemetry_tests` but with additional
# `gpu_skia_renderer_vulkan_passthrough_telemetry_tests`
targets.legacy_compound_suite(
    name = "gpu_all_linux_release_vulkan_telemetry_tests",
    basic_suites = [
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
        "gpu_skia_renderer_vulkan_passthrough_telemetry_tests",

        # Migrated individual tests below.
        # TODO(crbug.com/541312843): Remove this comment once all tests are
        # directly included.
        "legacy_info_collection_tests",
        "legacy_trace_test",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_fyi_only_mac_release_telemetry_tests",
    basic_suites = [
        "gpu_metal_passthrough_graphite_telemetry_tests",
        "gpu_webcodecs_metal_passthrough_graphite_telemetry_test",
        "gpu_webrtc_metal_passthrough_graphite_telemetry_test",
        "gpu_webgl2_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_metal_passthrough_graphite_telemetry_tests",
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",

        # Migrated individual tests below.
        # TODO(crbug.com/541312843): Remove this comment once all tests are
        # directly included.
        "legacy_info_collection_tests",
        "legacy_trace_test",
    ],
)

# This is functionally equivalent to the gpu_all_win_release_telemetry_tests
# bundle, but is defined separately since V8 needs to use legacy suites for now.
targets.legacy_compound_suite(
    name = "gpu_v8_win_release_telemetry_tests",
    basic_suites = [
        "gpu_passthrough_graphite_telemetry_tests",
        "gpu_passthrough_telemetry_tests",
        "gpu_webcodecs_telemetry_test",
        "gpu_webrtc_telemetry_test",
        "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
        "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",

        # Migrated individual tests below.
        # TODO(crbug.com/541312843): Remove this comment once all tests are
        # directly included.
        "legacy_info_collection_tests",
        "legacy_trace_test",
    ],
)

targets.legacy_compound_suite(
    name = "gpu_telemetry_tests_v8",
    basic_suites = [
        "gpu_webgl_conformance_validating_telemetry_tests",

        # Migrated individual tests below.
        # TODO(crbug.com/541312843): Remove this comment once all tests are
        # directly included.
        "legacy_context_lost_validating_tests",
        "legacy_expected_color_pixel_validating_test",
        "legacy_gpu_process_launch_tests",
        "legacy_hardware_accelerated_feature_tests",
        "legacy_info_collection_tests",
        "legacy_trace_test",
        "legacy_pixel_skia_gold_validating_test",
        "legacy_screenshot_sync_validating_tests",
        "legacy_webgl_conformance_gles_passthrough_tests",
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
    name = "linux_chromeos_isolated_scripts",
    basic_suites = [
        "blink_web_tests_ppapi_isolated_scripts",
        "chrome_sizes_suite",
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
