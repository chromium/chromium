# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Matrix Compound suite declarations

Matrix compound suites are a collection of basic suites that can be referenced
by a builder in //testing/buildbot/waterfalls.pyl and additionally support
expanding tests into multiple instances using variants (defined in
./variants.star). Suites also define a bundle containing the same tests as the
suite, so they can be used wherever a bundle is expected.

The legacy_ prefix denotes the ability for basic suites to be referenced in
//testing/buildbot. Once a suite is no longer referenced via //testing/buildbot,
targets.bundle can be used for grouping tests in a more flexible manner (mixing
test types and/or compile targets and arbitrary nesting). Named bundles are
defined in ./bundles.star.
"""

load("@chromium-luci//targets.star", "targets")

targets.legacy_matrix_compound_suite(
    name = "chromeos_vmlab_tests",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            mixins = [
                "skylab-shards-20",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_criticalstaging_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_disabled_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_system_friendly_gtests_vmlab": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_system_friendly_gtests_fails_vmlab": targets.legacy_matrix_config(
            # TODO: remove experimentals after stablization.
            mixins = [
                "experiments",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_vaapi_gtests": targets.legacy_matrix_config(
            # TODO: remove experimentals after stablization.
            mixins = [
                "experiments",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "chromeos_vmlab_tests_no_gtests",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_criticalstaging_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_disabled_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "chromeos_vmlab_tests_no_gtests_no_arc",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_criticalstaging_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_disabled_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            # TODO(b/353643755): Remove once ARC tests not compiled in.
            mixins = [
                "crosier-no-arc",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "chromeos_brya_skylab_tests",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_criticalstaging_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_disabled_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_system_friendly_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_vaapi_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "chromeos_jacuzzi_skylab_tests",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            mixins = [
                # jacuzzi is slow. So that we use more number of shards.
                "skylab-shards-30",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_criticalstaging_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_disabled_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "chromeos_octopus_skylab_tests",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "chromeos_trogdor_skylab_tests",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            mixins = [
                # trogdor is slow. So that we use more number of shards.
                "skylab-shards-20",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "chromeos_volteer_skylab_tests",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_criticalstaging_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_disabled_tast_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    # Tests scheduled via CTP for preuprev.
    name = "chromeos_ctp_preuprev_tests",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            mixins = [
                # Board with more capacity will run full tast test with many shards.
                "skylab-shards-30",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "chromeos_ctp_preuprev_tests_slow_boards",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            mixins = [
                # jacuzzi is slow. So that we use more number of shards.
                "skylab-shards-50",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "gpu_fyi_chromeos_brya_telemetry_tests",
    basic_suites = {
        "gpu_noop_sleep_telemetry_test": targets.legacy_matrix_config(
            variants = [
                "CROS_GPU_BRYA_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "gpu_fyi_chromeos_corsola_telemetry_tests",
    basic_suites = {
        "gpu_noop_sleep_telemetry_test": targets.legacy_matrix_config(
            variants = [
                "CROS_GPU_CORSOLA_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "gpu_fyi_chromeos_release_telemetry_tests_jacuzzi_skylab",
    basic_suites = {
        "gpu_common_and_optional_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_JACUZZI_RELEASE_LKGM",
            ],
        ),
        "gpu_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_JACUZZI_RELEASE_LKGM",
            ],
        ),
        "gpu_webcodecs_telemetry_test": targets.legacy_matrix_config(
            variants = [
                "CROS_JACUZZI_RELEASE_LKGM",
            ],
        ),
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_JACUZZI_RELEASE_LKGM",
            ],
        ),
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_JACUZZI_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "gpu_fyi_chromeos_skyrim_telemetry_tests",
    basic_suites = {
        "gpu_noop_sleep_telemetry_test": targets.legacy_matrix_config(
            variants = [
                "CROS_GPU_SKYRIM_RELEASE_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "model_validation_gce_matrix_tests",
    basic_suites = {
        "model_validation_tests_light_suite": targets.legacy_matrix_config(
            # Run model validation tests on GCE when possible to save bare
            # metal resources. This cannot be done for ondevice_stability_tests
            # because it requires a GPU.
            mixins = [
                "gce",
            ],
            variants = [
                "CHANNEL_BETA",
                "CHANNEL_DEV",
                "CHANNEL_STABLE",
            ],
        ),
        "ondevice_stability_tests_light_suite": targets.legacy_matrix_config(
            # For stability tests we need a GPU, so we run them on NVIDIA
            # GeForce GTX 1660s, which are most available in the intelligence
            # pool.
            mixins = [
                "nvidia_geforce_gtx_1660",
            ],
            variants = [
                "CHANNEL_BETA",
                "CHANNEL_DEV",
                "CHANNEL_STABLE",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "model_validation_matrix_tests",
    basic_suites = {
        "model_validation_tests_light_suite": targets.legacy_matrix_config(
            variants = [
                "CHANNEL_BETA",
                "CHANNEL_DEV",
                "CHANNEL_STABLE",
            ],
        ),
        "ondevice_stability_tests_light_suite": targets.legacy_matrix_config(
            variants = [
                "CHANNEL_BETA",
                "CHANNEL_DEV",
                "CHANNEL_STABLE",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_desktop_gtests",
    basic_suites = {
        "optimization_guide_nogpu_gtests": None,
        "optimization_guide_gpu_gtests": None,
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_ios_sim_gtests",
    basic_suites = {
        "optimization_guide_ios_unittests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_18_2",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_ios_device_gtests",
    basic_suites = {
        "optimization_guide_ios_unittests": targets.legacy_matrix_config(
            variants = [
                "IPHONE_13",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_mac_script_tests",
    basic_suites = {
        "model_validation_tests_suite": None,
        "model_validation_tests_light_suite": None,
        "ondevice_stability_tests_suite": None,
        "chrome_ai_wpt_tests_suite": None,
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_win_arm64_script_tests",
    basic_suites = {
        "model_validation_tests_suite": None,
        "model_validation_tests_light_suite": None,
        "ondevice_stability_tests_suite": None,
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_linux_gtests",
    basic_suites = {
        "optimization_guide_nogpu_gtests": targets.legacy_matrix_config(
            mixins = [
                "gce",
            ],
        ),
        "optimization_guide_gpu_gtests": targets.legacy_matrix_config(
            # TODO(b:322815244): Add AMD variant once driver issues are fixed.
            variants = [
                "NVIDIA_GEFORCE_GTX_1660",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_linux_script_tests",
    basic_suites = {
        "model_validation_tests_suite": targets.legacy_matrix_config(
            mixins = [
                "gce",
            ],
        ),
        "model_validation_tests_light_suite": targets.legacy_matrix_config(
            mixins = [
                "gce",
            ],
        ),
        "ondevice_stability_tests_suite": targets.legacy_matrix_config(
            variants = [
                "NVIDIA_GEFORCE_GTX_1660",
            ],
        ),
        "opt_target_coverage_test_suite": targets.legacy_matrix_config(
            mixins = [
                "gce",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_win_gtests",
    basic_suites = {
        "optimization_guide_nogpu_gtests": targets.legacy_matrix_config(
            mixins = [
                "gce",
            ],
        ),
        "optimization_guide_gpu_gtests": targets.legacy_matrix_config(
            variants = [
                "AMD_RADEON_RX_5500_XT",
                "INTEL_UHD_630_OR_770",
                "NVIDIA_GEFORCE_GTX_1660",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_win32_script_tests",
    basic_suites = {
        "model_validation_tests_suite": targets.legacy_matrix_config(
            mixins = [
                "gce",
            ],
        ),
        "model_validation_tests_light_suite": targets.legacy_matrix_config(
            mixins = [
                "gce",
            ],
        ),
        "ondevice_stability_tests_suite": targets.legacy_matrix_config(
            variants = [
                "AMD_RADEON_RX_5500_XT",
                "NVIDIA_GEFORCE_GTX_1660",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_win64_script_tests",
    basic_suites = {
        "model_validation_tests_suite": targets.legacy_matrix_config(
            mixins = [
                "gce",
            ],
        ),
        "model_validation_tests_light_suite": targets.legacy_matrix_config(
            mixins = [
                "gce",
            ],
        ),
        "ondevice_stability_tests_suite": targets.legacy_matrix_config(
            variants = [
                "AMD_RADEON_RX_5500_XT",
                "INTEL_UHD_630_OR_770",
                "NVIDIA_GEFORCE_GTX_1660",
            ],
        ),
    },
)
