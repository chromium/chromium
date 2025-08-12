# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.angle builder group."""

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/gpu.star", "gpu")
load("//lib/siso.star", "siso")
load("//lib/xcode.star", "xcode")

ci.defaults.set(
    executable = "recipe:angle_chromium",
    builder_group = "chromium.angle",
    pool = gpu.ci.POOL,
    gardener_rotations = gardener_rotations.ANGLE,
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.default(),
    properties = {
        "perf_dashboard_machine_group": "ChromiumANGLE",
    },
    service_account = gpu.ci.SERVICE_ACCOUNT,
    shadow_service_account = gpu.ci.SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
    thin_tester_cores = 2,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
        "swarming_containment_auto",
    ],
)

targets.settings_defaults.set(allow_script_tests = False)

consoles.console_view(
    name = "chromium.angle",
    ordering = {
        None: [
            "Android",
            "Fuchsia",
            "Linux",
            "Mac",
            "iOS",
            "Windows",
            "Perf",
        ],
        "*builder*": ["Builder"],
        "Android": "*builder*",
        "Fuchsia": "*builder*",
        "Linux": "*builder*",
        "Mac": "*builder*",
        "iOS": "*builder*",
        "Windows": "*builder*",
        "Perf": "*builder*",
    },
)

gpu.ci.linux_builder(
    name = "android-angle-chromium-arm64-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "android_builder_without_codecs",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "arm64",
            "static_angle",
            "android_fastbuild",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Android|Builder|Chromium",
        short_name = "arm64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "android-angle-chromium-arm64-pixel2",
    description_html = "Running Angle chromium tests on Pixel 2",
    parent = "android-angle-chromium-arm64-builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
        ],
        mixins = [
            "chromium_pixel_2_pie",
            "has_native_resultdb_integration",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android|Pixel2|Chromium",
        short_name = "arm64",
    ),
    contact_team_email = "angle-team@google.com",
)

gpu.ci.linux_builder(
    name = "fuchsia-angle-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
                "fuchsia",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "shared",
            "release",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "fuchsia",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "angle_end2end_tests",
            "angle_unittests",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Fuchsia|Builder|ANGLE",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

gpu.ci.linux_builder(
    name = "linux-angle-chromium-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Builder|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "linux-angle-chromium-intel",
    parent = "linux-angle-chromium-builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_common_gtests_passthrough",
            "gpu_angle_linux_telemetry_tests",
        ],
        mixins = [
            "linux_intel_uhd_630_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|Intel|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "linux-angle-chromium-nvidia",
    parent = "linux-angle-chromium-builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_common_gtests_passthrough",
            "gpu_angle_linux_telemetry_tests",
        ],
        mixins = [
            "linux_nvidia_gtx_1660_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux|NVIDIA|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

gpu.ci.mac_builder(
    name = "mac-angle-chromium-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Builder|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "mac-angle-chromium-amd",
    parent = "mac-angle-chromium-builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_common_gtests_passthrough",
            "gpu_angle_mac_telemetry_tests",
        ],
        mixins = [
            "mac_retina_amd_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|AMD|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "mac-angle-chromium-intel",
    parent = "mac-angle-chromium-builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_common_gtests_passthrough",
            "gpu_angle_mac_telemetry_tests",
        ],
        mixins = [
            "mac_mini_intel_gpu_stable",
        ],
        per_test_modifications = {
            "webgl2_conformance_gl_passthrough_ganesh_tests": targets.per_test_modification(
                mixins = targets.mixin(
                    # TODO(crbug.com/326277739): Remove this once we determine
                    # if this has an impact on ANGLE test stability.
                    args = [
                        "--jobs=2",
                    ],
                ),
                replacements = targets.replacements(
                    # Magic substitution happens after regular replacement, so
                    # remove it now since we are manually applying the number of
                    # jobs above.
                    args = {
                        targets.magic_args.GPU_PARALLEL_JOBS: None,
                    },
                ),
            ),
            "webgl2_conformance_metal_passthrough_graphite_tests": targets.per_test_modification(
                mixins = targets.mixin(
                    # TODO(crbug.com/326277739): Remove this once we determine
                    # if this has an impact on ANGLE test stability.
                    args = [
                        "--jobs=2",
                    ],
                ),
                replacements = targets.replacements(
                    # Magic substitution happens after regular replacement, so
                    # remove it now since we are manually applying the number of
                    # jobs above.
                    args = {
                        targets.magic_args.GPU_PARALLEL_JOBS: None,
                    },
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac|Intel|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

gpu.ci.mac_builder(
    name = "ios-angle-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "ios_simulator",
            "x64",
            "xctest",
        ],
    ),
    targets = targets.bundle(),
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "iOS|Builder|ANGLE",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
    xcode = xcode.xcode_default,
)

ci.thin_tester(
    name = "ios-angle-intel",
    parent = "ios-angle-builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_angle_ios_gtests",
        ],
        mixins = [
            "mac_mini_intel_gpu_stable",
            "has_native_resultdb_integration",
            "isolate_profile_data",
            "mac_toolchain",
            "out_dir_arg",
            "xcode_16_main",
            "xctest",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "iOS|Intel|ANGLE",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

gpu.ci.windows_builder(
    name = "win-angle-chromium-x64-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "win10-angle-chromium-x64-intel",
    parent = "win-angle-chromium-x64-builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_angle_win_intel_nvidia_telemetry_tests",
        ],
        mixins = [
            "win10_intel_uhd_630_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Intel|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

ci.thin_tester(
    name = "win10-angle-chromium-x64-nvidia",
    parent = "win-angle-chromium-x64-builder",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-angle-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_angle_win_intel_nvidia_telemetry_tests",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
        per_test_modifications = {
            # TODO(crbug.com/380431384): Re-enable when fixed
            "webgl_conformance_vulkan_passthrough_tests": targets.remove(
                reason = [
                    "crbug.com/380431384 flaky crashes in random tests",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|NVIDIA|Chromium",
        short_name = "x64",
    ),
    contact_team_email = "angle-team@google.com",
)

gpu.ci.windows_builder(
    name = "win-angle-chromium-x86-builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-angle-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "win",
            "x86",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "telemetry_gpu_integration_test",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows|Builder|Chromium",
        short_name = "x86",
    ),
    contact_team_email = "angle-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)
