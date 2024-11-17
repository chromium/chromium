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

targets.legacy_matrix_compound_suite(
    name = "chromeos_vmlab_tests",
    basic_suites = {
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            mixins = [
                "shards-20",
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
    name = "chromeos_jacuzzi_rel_skylab_tests",
    basic_suites = {
        # After the builder gets stabilized, 'chromeos_device_only_gtests' will
        # be tried to be replaced with 'chromeos_system_friendly_gtests'.
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_PUBLIC_LKGM",
            ],
        ),
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            mixins = [
                "chromeos-tast-public-builder",
                # jacuzzi is slow. So that we use more number of shards.
                "shards-30",
            ],
            variants = [
                "CROS_PUBLIC_LKGM",
            ],
        ),
        "chromeos_chrome_criticalstaging_tast_tests": targets.legacy_matrix_config(
            mixins = [
                "chromeos-tast-public-builder",
            ],
            variants = [
                "CROS_PUBLIC_LKGM",
            ],
        ),
        "chromeos_chrome_disabled_tast_tests": targets.legacy_matrix_config(
            mixins = [
                "chromeos-tast-public-builder",
            ],
            variants = [
                "CROS_PUBLIC_LKGM",
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
                "shards-30",
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
    name = "chromeos_octopus_rel_skylab_tests",
    basic_suites = {
        # After the builder gets stabilized, 'chromeos_device_only_gtests' will
        # be tried to be replaced with 'chromeos_system_friendly_gtests'.
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_PUBLIC_LKGM",
            ],
        ),
        "chromeos_chrome_all_tast_tests": targets.legacy_matrix_config(
            mixins = [
                "chromeos-tast-public-builder",
            ],
            variants = [
                "CROS_PUBLIC_LKGM",
            ],
        ),
        "chromeos_chrome_criticalstaging_tast_tests": targets.legacy_matrix_config(
            mixins = [
                "chromeos-tast-public-builder",
            ],
            variants = [
                "CROS_PUBLIC_LKGM",
            ],
        ),
        "chromeos_chrome_disabled_tast_tests": targets.legacy_matrix_config(
            mixins = [
                "chromeos-tast-public-builder",
            ],
            variants = [
                "CROS_PUBLIC_LKGM",
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
                "shards-20",
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
                "shards-30",
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
                "shards-50",
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
    name = "dawn_chromeos_release_tests_volteer_skylab",
    basic_suites = {
        # gtests
        "gpu_common_gtests_passthrough": targets.legacy_matrix_config(
            variants = [
                "CROS_VOLTEER_PUBLIC_RELEASE_ASH_LKGM",
            ],
        ),
        "gpu_dawn_gtests": targets.legacy_matrix_config(
            variants = [
                "CROS_VOLTEER_PUBLIC_RELEASE_ASH_LKGM",
            ],
        ),
        "gpu_dawn_gtests_with_validation": targets.legacy_matrix_config(
            variants = [
                "CROS_VOLTEER_PUBLIC_RELEASE_ASH_LKGM",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "dawn_chromeos_release_telemetry_tests_volteer_skylab",
    basic_suites = {
        # TODO(crbug.com/340815322): Add gpu_dawn_webgpu_compat_cts once
        # compat works properly on ChromeOS.
        "gpu_dawn_webgpu_cts": targets.legacy_matrix_config(
            variants = [
                "CROS_VOLTEER_PUBLIC_RELEASE_ASH_LKGM",
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
    name = "gpu_fyi_chromeos_release_gtests_volteer_skylab",
    basic_suites = {
        # gpu_angle_unit_gtests and gpu_desktop_specific_gtests should also be
        # enabled here, but are removed for various reasons. See the definition
        # for gpu_fyi_chromeos_release_gtests in compound_suites.star for more
        # information.
        "gpu_common_gtests_passthrough": targets.legacy_matrix_config(
            variants = [
                "CROS_VOLTEER_PUBLIC_RELEASE_ASH_LKGM",
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
    name = "gpu_fyi_chromeos_release_telemetry_tests_volteer_skylab",
    basic_suites = {
        "gpu_common_and_optional_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_VOLTEER_PUBLIC_RELEASE_ASH_LKGM",
            ],
        ),
        "gpu_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_VOLTEER_PUBLIC_RELEASE_ASH_LKGM",
            ],
        ),
        "gpu_webcodecs_telemetry_test": targets.legacy_matrix_config(
            variants = [
                "CROS_VOLTEER_PUBLIC_RELEASE_ASH_LKGM",
            ],
        ),
        "gpu_webgl_conformance_gles_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_VOLTEER_PUBLIC_RELEASE_ASH_LKGM",
            ],
        ),
        "gpu_webgl2_conformance_gles_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "CROS_VOLTEER_PUBLIC_RELEASE_ASH_LKGM",
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
                "SIM_IPHONE_14_18_0",
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
    name = "optimization_guide_win_script_tests",
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
