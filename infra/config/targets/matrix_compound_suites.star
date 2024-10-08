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
    name = "android_fieldtrial_rel_webview_tests",
    basic_suites = {
        "fieldtrial_android_tests": None,
        "webview_bot_instrumentation_test_apk_gtest": targets.legacy_matrix_config(
            variants = [
                "DISABLE_FIELD_TRIAL_CONFIG",
                "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR",
                "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR",
            ],
        ),
        "webview_trichrome_64_cts_field_trial_tests": targets.legacy_matrix_config(
            variants = [
                "DISABLE_FIELD_TRIAL_CONFIG",
                "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR",
                "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR",
            ],
        ),
        "webview_ui_instrumentation_tests": targets.legacy_matrix_config(
            variants = [
                "DISABLE_FIELD_TRIAL_CONFIG",
                "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR",
                "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR",
            ],
        ),
        "system_webview_shell_instrumentation_tests": targets.legacy_matrix_config(
            variants = [
                "DISABLE_FIELD_TRIAL_CONFIG_WEBVIEW_COMMANDLINE",
                "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR_WEBVIEW_COMMANDLINE",
                "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR_WEBVIEW_COMMANDLINE",
            ],
        ),
    },
)

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
            mixins = [
                "skylab-cft",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            mixins = [
                "skylab-cft",
            ],
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
                "skylab-cft",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            mixins = [
                "skylab-cft",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            mixins = [
                "skylab-cft",
            ],
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
            mixins = [
                "skylab-cft",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_criticalstaging_tast_tests": targets.legacy_matrix_config(
            mixins = [
                "skylab-cft",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_chrome_disabled_tast_tests": targets.legacy_matrix_config(
            mixins = [
                "skylab-cft",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_device_only_gtests": targets.legacy_matrix_config(
            mixins = [
                "skylab-cft",
            ],
            variants = [
                "CROS_RELEASE_LKGM",
            ],
        ),
        "chromeos_integration_tests_suite": targets.legacy_matrix_config(
            mixins = [
                "skylab-cft",
            ],
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
    name = "fieldtrial_ios_simulator_tests",
    basic_suites = {
        "ios_eg2_cq_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
                "disable_field_trial_config_for_earl_grey",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPHONE_14_17_5",
            ],
        ),
        "ios_eg2_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
                "disable_field_trial_config_for_earl_grey",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPHONE_14_17_5",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "gpu_angle_ios_gtests",
    basic_suites = {
        "gpu_angle_ios_end2end_gtests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_14_18_0",
            ],
        ),
        "gpu_angle_ios_white_box_gtests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_14_18_0",
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
    name = "ios17_beta_simulator_tests",
    basic_suites = {
        "ios_common_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_18_1",
                "SIM_IPAD_AIR_5TH_GEN_18_1",
            ],
        ),
        "ios_eg2_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPHONE_14_18_1",
                "SIM_IPAD_AIR_5TH_GEN_18_1",
            ],
        ),
        "ios_eg2_cq_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPHONE_14_18_1",
                "SIM_IPAD_AIR_5TH_GEN_18_1",
            ],
        ),
        "ios_screen_size_dependent_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_18_1",
                "SIM_IPHONE_SE_3RD_GEN_18_1",
                "SIM_IPAD_AIR_5TH_GEN_18_1",
            ],
        ),
        "ios_crash_xcuitests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_14_18_1",
                "SIM_IPAD_AIR_5TH_GEN_18_1",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "ios17_sdk_simulator_tests",
    basic_suites = {
        "ios_common_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_18_1",
                "SIM_IPAD_AIR_5TH_GEN_18_1",
            ],
        ),
        "ios_eg2_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_14_18_1",
                "SIM_IPAD_AIR_5TH_GEN_18_1",
            ],
        ),
        "ios_eg2_cq_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPAD_AIR_5TH_GEN_18_1",
                "SIM_IPHONE_14_18_1",
            ],
        ),
        "ios_screen_size_dependent_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_18_1",
                "SIM_IPHONE_SE_3RD_GEN_18_1",
                "SIM_IPAD_AIR_5TH_GEN_18_1",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "ios18_beta_simulator_tests",
    basic_suites = {
        "ios_common_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_15_18_0",
            ],
        ),
        "ios_eg2_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPAD_10TH_GEN_18_0",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
            ],
        ),
        "ios_eg2_cq_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_10TH_GEN_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
            ],
        ),
        "ios_screen_size_dependent_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_15_18_0",
                "SIM_IPHONE_15_PRO_MAX_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
            ],
        ),
        "ios_crash_xcuitests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_15_18_0",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "ios18_sdk_simulator_tests",
    basic_suites = {
        "ios_common_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_15_18_0",
                "SIM_IPHONE_14_17_5",
            ],
        ),
        "ios_crash_xcuitests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_15_18_0",
                "SIM_IPHONE_14_17_5",
            ],
        ),
        "ios_eg2_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPHONE_15_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
            ],
        ),
        "ios_eg2_cq_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPHONE_15_18_0",
                "SIM_IPHONE_14_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
            ],
        ),
        "ios_screen_size_dependent_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_15_18_0",
                "SIM_IPHONE_SE_3RD_GEN_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "ios_asan_tests",
    basic_suites = {
        "ios_common_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
            ],
        ),
        "ios_screen_size_dependent_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "ios_blink_dbg_tests",
    basic_suites = {
        "ios_blink_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
    },
)

# This suite is a union of ios_simulator_tests and
# ios_simulator_full_configs_tests.
targets.legacy_matrix_compound_suite(
    name = "ios_code_coverage_tests",
    basic_suites = {
        "ios_common_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_16_4",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        "ios_eg2_cq_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_14_16_4",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_AIR_5TH_GEN_16_4",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
                "SIM_IPAD_PRO_6TH_GEN_16_4",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
            ],
        ),
        "ios_eg2_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_14_16_4",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_PRO_6TH_GEN_16_4",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
            ],
        ),
        "ios_screen_size_dependent_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_16_4",
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_PRO_6TH_GEN_16_4",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "ios_m1_simulator_tests",
    basic_suites = {
        "ios_common_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
            ],
        ),
        "ios_eg2_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
            ],
        ),
        "ios_eg2_cq_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
                "record_failed_tests",
            ],
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPAD_PRO_6TH_GEN_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_PRO_7TH_GEN_18_0",
            ],
        ),
        "ios_screen_size_dependent_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_PRO_MAX_17_5",
                "SIM_IPHONE_14_17_5",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPHONE_15_PRO_MAX_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "ios_webkit_tot_tests",
    basic_suites = {
        "ios_common_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
            ],
        ),
        "ios_eg2_cq_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
            ],
        ),
        "ios_eg2_tests": targets.legacy_matrix_config(
            mixins = [
                "xcodebuild_sim_runner",
            ],
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
            ],
        ),
        "ios_screen_size_dependent_tests": targets.legacy_matrix_config(
            variants = [
                "SIM_IPHONE_14_17_5",
                "SIM_IPAD_AIR_5TH_GEN_17_5",
                "SIM_IPHONE_15_18_0",
                "SIM_IPAD_AIR_6TH_GEN_18_0",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "linux_optional_gpu_tests_rel_gpu_telemetry_tests",
    basic_suites = {
        "gpu_common_and_optional_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "LINUX_INTEL_UHD_630_STABLE",
                "LINUX_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_webcodecs_telemetry_test": targets.legacy_matrix_config(
            variants = [
                "LINUX_INTEL_UHD_630_STABLE",
                "LINUX_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_webgl2_conformance_gl_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "LINUX_INTEL_UHD_630_STABLE",
                "LINUX_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_webgl_conformance_gl_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "LINUX_INTEL_UHD_630_STABLE",
                "LINUX_NVIDIA_GTX_1660_STABLE",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "mac_optional_gpu_tests_rel_gpu_telemetry_tests",
    basic_suites = {
        "gpu_common_and_optional_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
                "MAC_RETINA_NVIDIA_GPU_STABLE",
            ],
        ),
        "gpu_gl_passthrough_ganesh_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        "gpu_metal_passthrough_ganesh_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        "gpu_webcodecs_gl_passthrough_ganesh_telemetry_test": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
                "MAC_RETINA_NVIDIA_GPU_STABLE",
            ],
        ),
        "gpu_webcodecs_metal_passthrough_ganesh_telemetry_test": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        "gpu_webcodecs_metal_passthrough_graphite_telemetry_test": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        "gpu_webgl2_conformance_metal_passthrough_graphite_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        "gpu_webgl_conformance_metal_passthrough_ganesh_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
        "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "mac_optional_gpu_tests_rel_gtests",
    basic_suites = {
        "gpu_fyi_and_optional_non_linux_gtests": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
                "MAC_RETINA_NVIDIA_GPU_STABLE",
            ],
        ),
        "gpu_fyi_mac_specific_gtests": targets.legacy_matrix_config(
            variants = [
                "MAC_MINI_INTEL_GPU_STABLE",
                "MAC_RETINA_AMD_GPU_STABLE",
                "MAC_RETINA_NVIDIA_GPU_STABLE",
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
    name = "optimization_guide_mac_script_tests",
    basic_suites = {
        "model_validation_tests_suite": None,
        "model_validation_tests_light_suite": None,
        "ondevice_quality_tests_suite": None,
        "ondevice_stability_tests_suite": None,
        "chrome_ai_wpt_tests_suite": None,
    },
)

targets.legacy_matrix_compound_suite(
    name = "optimization_guide_win_arm64_script_tests",
    basic_suites = {
        "model_validation_tests_suite": None,
        "model_validation_tests_light_suite": None,
        "ondevice_quality_tests_suite": None,
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
        "ondevice_quality_tests_suite": targets.legacy_matrix_config(
            variants = [
                "NVIDIA_GEFORCE_GTX_1660",
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
        "ondevice_quality_tests_suite": targets.legacy_matrix_config(
            variants = [
                "AMD_RADEON_RX_5500_XT",
                "INTEL_UHD_630_OR_770",
                "NVIDIA_GEFORCE_GTX_1660",
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

targets.legacy_matrix_compound_suite(
    name = "win_optional_gpu_tests_rel_gpu_telemetry_tests",
    basic_suites = {
        "gpu_common_and_optional_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_passthrough_graphite_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_webcodecs_telemetry_test": targets.legacy_matrix_config(
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests": targets.legacy_matrix_config(
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "win_optional_gpu_tests_rel_gtests",
    basic_suites = {
        "gpu_default_and_optional_win_media_foundation_specific_gtests": targets.legacy_matrix_config(
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
            ],
        ),
        "gpu_default_and_optional_win_specific_gtests": targets.legacy_matrix_config(
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_fyi_and_optional_non_linux_gtests": targets.legacy_matrix_config(
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
        "gpu_fyi_and_optional_win_specific_gtests": targets.legacy_matrix_config(
            variants = [
                "WIN10_INTEL_UHD_630_STABLE",
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
    },
)

targets.legacy_matrix_compound_suite(
    name = "win_optional_gpu_tests_rel_isolated_scripts",
    basic_suites = {
        "gpu_command_buffer_perf_passthrough_isolated_scripts": targets.legacy_matrix_config(
            variants = [
                "WIN10_NVIDIA_GTX_1660_STABLE",
            ],
        ),
    },
)
