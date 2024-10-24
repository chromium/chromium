# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.dawn builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "cpu", "gardener_rotations", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.dawn",
    pool = ci.gpu.POOL,
    gardener_rotations = gardener_rotations.DAWN,
    contact_team_email = "chrome-gpu-infra@google.com",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    service_account = ci.gpu.SERVICE_ACCOUNT,
    shadow_service_account = ci.gpu.SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
    thin_tester_cores = 2,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
        "timeout_30m",
    ],
)

consoles.console_view(
    name = "chromium.dawn",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
        branches.selector.DESKTOP_BRANCHES,
    ],
    ordering = {
        None: ["ToT"],
        "*builder*": ["Builder"],
        "*cpu*": consoles.ordering(short_names = ["x86"]),
        "ToT|Mac": "*builder*",
        "ToT|Windows|Builder": "*cpu*",
        "ToT|Windows|Intel": "*cpu*",
        "ToT|Windows|Nvidia": "*cpu*",
        "DEPS|Mac": "*builder*",
        "DEPS|Windows|Builder": "*cpu*",
        "DEPS|Windows|Intel": "*cpu*",
        "DEPS|Windows|Nvidia": "*cpu*",
    },
)

ci.gpu.linux_builder(
    name = "Dawn Chromium Presubmit",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
        branches.selector.DESKTOP_BRANCHES,
    ],
    description_html = "Runs Chromium presubmit tests on Dawn CLs",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
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
    ),
    # This builder shouldn't be actually compiling anything, but it does need
    # GN args set in order to generate and isolate targets.
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_common_isolated_scripts",
        ],
        mixins = [
            "linux-jammy",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Presubmit",
        short_name = "psm",
    ),
    execution_timeout = 30 * time.minute,
)

ci.gpu.linux_builder(
    name = "Dawn Linux x64 Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Builder",
        short_name = "x64",
    ),
)

ci.gpu.linux_builder(
    name = "Dawn Linux x64 DEPS Builder",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Linux|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.linux_builder(
    name = "Dawn Android arm DEPS Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "arm",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Android|Builder",
        short_name = "arm",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "Dawn Android arm64 DEPS Builder",
    description_html = "Builds Android arm64 binaries using DEPS-ed in Dawn",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "arm64",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Android|Builder",
        short_name = "a64",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "Dawn Android arm DEPS Release (Nexus 5X)",
    triggered_by = ["ci/Dawn Android arm DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "chromium_nexus_5x_oreo",
            "has_native_resultdb_integration",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Android",
        short_name = "n5x",
    ),
)

ci.thin_tester(
    name = "Dawn Android arm DEPS Release (Pixel 4)",
    triggered_by = ["ci/Dawn Android arm DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_android_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_android_isolated_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_pixel_4_stable",
        ],
        per_test_modifications = {
            "dawn_end2end_skip_validation_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_end2end_validation_layers_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_end2end_wire_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_perf_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "gl_tests_passthrough": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "gl_unittests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_blink_web_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_blink_web_tests_with_backend_validation": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = [
                    "We only need coverage on one GPU per OS, so remove from lower capacity",
                    "configs. Additionally, shared workers are not supported on Android.",
                ],
            ),
            "webgpu_cts_tests": targets.mixin(
                ci_only = True,
            ),
            "webgpu_cts_with_validation_tests": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Android",
        short_name = "p4",
    ),
)

ci.thin_tester(
    name = "Dawn Android arm64 DEPS Release (Pixel 6)",
    triggered_by = ["ci/Dawn Android arm64 DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_android_compat_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_android_isolated_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_pixel_6_stable",
        ],
        per_test_modifications = {
            "dawn_end2end_skip_validation_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_end2end_validation_layers_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_end2end_wire_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_perf_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "gl_tests_passthrough": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "gl_unittests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_blink_web_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_blink_web_tests_with_backend_validation": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = [
                    "We only need coverage on one GPU per OS, so remove from lower capacity",
                    "configs. Additionally, shared workers are not supported on Android.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Android",
        short_name = "p6",
    ),
)

ci.thin_tester(
    name = "Dawn Linux x64 DEPS Release (Intel UHD 630)",
    branch_selector = branches.selector.LINUX_BRANCHES,
    triggered_by = ["ci/Dawn Linux x64 DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_compat_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "linux_intel_uhd_630_stable",
        ],
        per_test_modifications = {
            # TODO(crbug.com/40932096): Enable on Intel trybots once there is
            # sufficient capacity.
            "webgpu_cts_compat_tests": targets.mixin(
                ci_only = True,
                experiment_percentage = 100,
            ),
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            # ci_only for bots where capacity is constrained.
            "webgpu_cts_with_validation_tests": targets.mixin(
                ci_only = True,
            ),
            # ci_only for bots where capacity is constrained.
            "webgpu_swiftshader_web_platform_cts_with_validation_tests": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Linux|Intel",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Dawn Linux x64 DEPS Release (NVIDIA)",
    branch_selector = branches.selector.LINUX_BRANCHES,
    triggered_by = ["ci/Dawn Linux x64 DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_compat_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
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
        category = "DEPS|Linux|Nvidia",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.linux_builder(
    name = "Dawn Linux TSAN Release",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "tsan",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        # This bot doesn't run any browser-based tests
        targets = [
            "gpu_dawn_tsan_gtests",
        ],
        mixins = [
            "linux_nvidia_gtx_1660_stable",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.LINUX,
        # This bot doesn't run any Telemetry-based tests so doesn't
        # need the browser_config parameter.
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|TSAN",
        short_name = "x64",
    ),
    # Serially executed tests + TSAN = more than the default timeout needed in
    # order to prevent build timeouts.
    execution_timeout = 6 * time.hour,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "Dawn Android arm Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "arm",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Android|Builder",
        short_name = "arm",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.gpu.linux_builder(
    name = "Dawn Android arm64 Builder",
    description_html = "Builds Android arm64 binaries using ToT Dawn",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "arm64",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Android|Builder",
        short_name = "a64",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "Dawn Android arm Release (Nexus 5X)",
    triggered_by = ["ci/Dawn Android arm Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "chromium_nexus_5x_oreo",
            "has_native_resultdb_integration",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Android",
        short_name = "n5x",
    ),
)

ci.thin_tester(
    name = "Dawn Android arm Release (Pixel 4)",
    triggered_by = ["ci/Dawn Android arm Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_android_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_android_isolated_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_pixel_4_stable",
        ],
        per_test_modifications = {
            "dawn_end2end_skip_validation_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_end2end_validation_layers_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_end2end_wire_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_perf_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "gl_tests_passthrough": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "gl_unittests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_blink_web_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_blink_web_tests_with_backend_validation": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = [
                    "We only need coverage on one GPU per OS, so remove from lower capacity",
                    "configs. Additionally, shared workers are not supported on Android.",
                ],
            ),
            "webgpu_cts_tests": targets.mixin(
                ci_only = True,
            ),
            "webgpu_cts_with_validation_tests": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Android",
        short_name = "p4",
    ),
)

ci.thin_tester(
    name = "Dawn Android arm64 Release (Pixel 6)",
    triggered_by = ["ci/Dawn Android arm64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_android_compat_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_android_isolated_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_pixel_6_stable",
        ],
        per_test_modifications = {
            "dawn_end2end_skip_validation_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_end2end_validation_layers_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_end2end_wire_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_perf_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "gl_tests_passthrough": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "gl_unittests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_blink_web_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_blink_web_tests_with_backend_validation": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = [
                    "We only need coverage on one GPU per OS, so remove from lower capacity",
                    "configs. Additionally, shared workers are not supported on Android.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Android",
        short_name = "p6",
    ),
)

ci.thin_tester(
    name = "Dawn Android arm64 Experimental Release (Pixel 6)",
    description_html = "Runs ToT Dawn tests on experimental Pixel 6 configs",
    triggered_by = ["ci/Dawn Android arm64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental OS version is identical to the stable version,
        # the gpu_noop_sleep_telemetry_test test should be used. Otherwise, this
        # should have the same test_suites as 'Dawn Android arm64 Release (Pixel 6)'.
        targets = [
            "gpu_dawn_android_compat_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_android_isolated_scripts",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_pixel_6_experimental",
            "limited_capacity_bot",
        ],
        per_test_modifications = {
            "dawn_end2end_skip_validation_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_end2end_validation_layers_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_end2end_wire_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "dawn_perf_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "gl_tests_passthrough": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "gl_unittests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_blink_web_tests": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_blink_web_tests_with_backend_validation": targets.remove(
                reason = "TODO(crbug.com/40238674): Enable once it's shown to work on Android.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = [
                    "We only need coverage on one GPU per OS, so remove from lower capacity",
                    "configs. Additionally, shared workers are not supported on Android.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
    ),
    # TODO(crbug.com/41489949): This config is experimental. It is not part of
    # the WebGPU CTS roller for capacity reasons, so it goes red with each roll.
    # Gardeners don't need to fix this, so exclude it from Sheriff-o-Matic.
    # It should be added back to SoM once the roller runs it.
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Android",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Dawn Android arm64 Release (Samsung S24)",
    description_html = "Runs ToT Dawn tests on Samsung S24 devices",
    triggered_by = ["ci/Dawn Android arm64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "arm64_builder_rel_mb",
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # TODO(crbug.com/333424893): Enable tests.
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_samsung_s24_stable",
            "limited_capacity_bot",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Android",
        short_name = "s24",
    ),
)

ci.gpu.linux_builder(
    name = "Dawn ChromeOS Skylab Release (volteer)",
    description_html = "Runs ToT Dawn tests on Skylab-hosted volteer devices",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
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
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "volteer",
            ],
        ),
        run_tests_serially = True,
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chromium-ci-skylab",
            gs_extra = "chromeos_gpu",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "gpu_tests",
            "chromeos_device",
            "volteer",
            "ozone_headless",
            "release_builder",
            "try_builder",
            "remoteexec",
            "dcheck_off",
            "no_symbols",
            "is_skylab",
            "chromeos",
            "x64",
        ],
    ),
    # TODO(crbug.com/40942991): This config is experimental and currently
    # is too difficult for gardeners to keep green.
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "ChromeOS|Intel",
        short_name = "vlt",
    ),
    execution_timeout = 6 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "Dawn Linux x64 Experimental Release (Intel UHD 630)",
    description_html = "Runs ToT Dawn tests on experimental Linux/UHD 630 configs",
    triggered_by = ["Dawn Linux x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental OS version is identical to the stable version,
        # the gpu_noop_sleep_telemetry_test test should be used. Otherwise, this
        # should have the same test_suites as 'Dawn Linux x64 Release (Intel UHD
        # 630)'.
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "linux_intel_uhd_630_experimental",
            "limited_capacity_bot",
        ],
        per_test_modifications = {
            # "gl_tests_passthrough": targets.mixin(
            #     args = [
            #         "--test-launcher-filter-file=../../testing/buildbot/filters/linux.uhd_630.gl_tests_passthrough.filter",
            #     ],
            # ),
            # # TODO(crbug.com/40932096): Enable on Intel trybots once there is
            # # sufficient capacity.
            # "webgpu_cts_compat_tests": targets.mixin(
            #     ci_only = True,
            #     experiment_percentage = 100,
            # ),
            # "webgpu_cts_dedicated_worker_tests": targets.remove(
            #     reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            # ),
            # "webgpu_cts_service_worker_tests": targets.remove(
            #     reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            # ),
            # "webgpu_cts_shared_worker_tests": targets.remove(
            #     reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            # ),
            # # ci_only for bots where capacity is constrained.
            # "webgpu_cts_with_validation_tests": targets.mixin(
            #     ci_only = True,
            # ),
            # # ci_only for bots where capacity is constrained.
            # "webgpu_swiftshader_web_platform_cts_with_validation_tests": targets.mixin(
            #     ci_only = True,
            # ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "ToT|Linux|Intel",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Dawn Linux x64 Experimental Release (NVIDIA GTX 1660)",
    description_html = "Runs ToT Dawn tests on experimental Linux/GTX 1660 configs",
    triggered_by = ["Dawn Linux x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_compat_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "linux_nvidia_gtx_1660_experimental",
            "limited_capacity_bot",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Nvidia",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Dawn Linux x64 Release (Intel UHD 630)",
    triggered_by = ["Dawn Linux x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_compat_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "linux_intel_uhd_630_stable",
        ],
        per_test_modifications = {
            # TODO(crbug.com/40932096): Enable on Intel trybots once there is
            # sufficient capacity.
            "webgpu_cts_compat_tests": targets.mixin(
                ci_only = True,
                experiment_percentage = 100,
            ),
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            # ci_only for bots where capacity is constrained.
            "webgpu_cts_with_validation_tests": targets.mixin(
                ci_only = True,
            ),
            # ci_only for bots where capacity is constrained.
            "webgpu_swiftshader_web_platform_cts_with_validation_tests": targets.mixin(
                ci_only = True,
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Intel",
        short_name = "630",
    ),
)

ci.thin_tester(
    name = "Dawn Linux x64 Release (Intel UHD 770)",
    description_html = "Runs ToT Dawn tests on 12th gen Intel CPUs with UHD 770 GPUs",
    triggered_by = ["Dawn Linux x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_compat_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "linux_intel_uhd_770_stable",
        ],
        per_test_modifications = {
            "gl_tests_passthrough": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/linux.uhd_770.gl_tests_passthrough.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Intel",
        short_name = "770",
    ),
)

ci.thin_tester(
    name = "Dawn Linux x64 Release (NVIDIA)",
    triggered_by = ["Dawn Linux x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_compat_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
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
        category = "ToT|Linux|Nvidia",
        short_name = "x64",
    ),
)

ci.gpu.mac_builder(
    name = "Dawn Mac arm64 Builder",
    description_html = "Compiles ToT Mac binaries for arm64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-dawn-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "arm64",
            "gpu_tests",
            "mac",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|Builder",
        short_name = "arm64",
    ),
)

ci.gpu.mac_builder(
    name = "Dawn Mac arm64 DEPS Builder",
    branch_selector = branches.selector.MAC_BRANCHES,
    description_html = "Compiles DEPSed Mac binaries for arm64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-dawn-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "arm64",
            "gpu_tests",
            "mac",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Mac|Builder",
        short_name = "arm64",
    ),
)

ci.thin_tester(
    name = "Dawn Mac arm64 DEPS Release (Apple M2)",
    branch_selector = branches.selector.MAC_BRANCHES,
    description_html = "Tests Dawn on M2 machines with DEPSed binaries",
    triggered_by = ["Dawn Mac arm64 DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough_macos",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "mac_arm64_apple_m2_retina_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Mac",
        short_name = "arm64",
    ),
)

ci.thin_tester(
    name = "Dawn Mac arm64 Experimental Release (Apple M2)",
    description_html = "Tests Dawn on experimental M2 machines with ToT binaries",
    triggered_by = ["Dawn Mac arm64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental OS version is identical to the stable version,
        # the gpu_noop_sleep_telemetry_test test should be used. Otherwise, this
        # should have the same test_suites as 'Dawn Mac arm64 Release (Apple
        # M2)'.
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "mac_arm64_apple_m2_retina_gpu_experimental",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "ToT|Mac",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Dawn Mac arm64 Release (Apple M2)",
    description_html = "Tests Dawn on M2 machines with ToT binaries",
    triggered_by = ["Dawn Mac arm64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough_macos",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "mac_arm64_apple_m2_retina_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac",
        short_name = "arm64",
    ),
)

ci.gpu.mac_builder(
    name = "Dawn Mac x64 Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "x64",
            "gpu_tests",
            "mac",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|Builder",
        short_name = "x64",
    ),
)

ci.gpu.mac_builder(
    name = "Dawn Mac x64 DEPS Builder",
    branch_selector = branches.selector.MAC_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "x64",
            "gpu_tests",
            "mac",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Mac|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
)

# Note that the Mac testers are all thin Linux VMs, triggering jobs on the
# physical Mac hardware in the Swarming pool which is why they run on linux
ci.thin_tester(
    name = "Dawn Mac x64 DEPS Release (AMD)",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/Dawn Mac x64 DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough_macos",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "mac_retina_amd_gpu_stable",
        ],
        per_test_modifications = {
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Mac|AMD",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Dawn Mac x64 DEPS Release (Intel)",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/Dawn Mac x64 DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough_macos",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "mac_mini_intel_gpu_stable",
        ],
        per_test_modifications = {
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Mac|Intel",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Dawn Mac x64 Experimental Release (AMD)",
    triggered_by = ["Dawn Mac x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental OS version is identical to the stable version,
        # the gpu_noop_sleep_telemetry_test test should be used. Otherwise, this
        # should have the same test_suites as 'Dawn Mac x64 Release (AMD)'.
        targets = [
            "gpu_dawn_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough_macos",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "limited_capacity_bot",
            "mac_retina_amd_gpu_experimental",
        ],
        per_test_modifications = {
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|AMD",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Dawn Mac x64 Experimental Release (Intel)",
    triggered_by = ["Dawn Mac x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental OS version is identical to the stable version,
        # the gpu_noop_sleep_telemetry_test test should be used. Otherwise, this
        # should have the same test_suites as 'Dawn Mac x64 Release (Intel)'.
        targets = [
            "gpu_dawn_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough_macos",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "limited_capacity_bot",
            "mac_mini_intel_gpu_experimental",
        ],
        per_test_modifications = {
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|Intel",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Dawn Mac x64 Release (AMD)",
    triggered_by = ["Dawn Mac x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough_macos",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "mac_retina_amd_gpu_stable",
        ],
        per_test_modifications = {
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|AMD",
        short_name = "x64",
    ),
)

ci.thin_tester(
    name = "Dawn Mac x64 Release (Intel)",
    triggered_by = ["Dawn Mac x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_tests",
            "gpu_dawn_integration_gtests_passthrough_macos",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "mac_mini_intel_gpu_stable",
        ],
        per_test_modifications = {
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|Intel",
        short_name = "x64",
    ),
)

ci.gpu.windows_builder(
    name = "Dawn Win10 x64 ASAN Builder",
    # One build every 2 hours.
    schedule = "0 */2 * * *",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
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
        build_gs_bucket = "chromium-dawn-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_use_built_dxc",
            "dawn_enable_opengles",
            "asan",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Builder",
        short_name = "asn",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "Dawn Win10 x64 ASAN Release (Intel)",
    triggered_by = ["ci/Dawn Win10 x64 ASAN Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_webgpu_cts_asan",
            "gpu_dawn_integration_asan_gtests_passthrough",
            "gpu_dawn_asan_isolated_scripts",
        ],
        mixins = [
            "win10_intel_uhd_630_stable",
        ],
        per_test_modifications = {
            "dawn_end2end_implicit_device_sync_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "dawn_end2end_no_dxc_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "dawn_end2end_skip_validation_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "dawn_end2end_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "dawn_end2end_wire_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_fxc_tests": targets.mixin(
                # ASAN builds taking a bit longer so needs extra shards to not timeout.
                swarming = targets.swarming(
                    shards = 14,
                ),
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_tests": targets.mixin(
                # ASAN builds taking a bit longer so needs extra shards to not timeout.
                swarming = targets.swarming(
                    shards = 16,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|x64|Intel",
        short_name = "asn",
    ),
    # Building DXC from source + ASAN results in longer run times, so
    # increase default timeout.
    execution_timeout = 4 * time.hour,
)

ci.thin_tester(
    name = "Dawn Win10 x64 ASAN Release (NVIDIA)",
    triggered_by = ["ci/Dawn Win10 x64 ASAN Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_webgpu_cts_asan",
            "gpu_dawn_integration_asan_gtests_passthrough",
            "gpu_dawn_asan_isolated_scripts",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
        per_test_modifications = {
            "dawn_end2end_implicit_device_sync_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "dawn_end2end_no_dxc_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "dawn_end2end_skip_validation_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "dawn_end2end_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "dawn_end2end_wire_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|x64|Nvidia",
        short_name = "asn",
    ),
    # Building DXC from source + ASAN results in longer run times, so
    # increase default timeout.
    execution_timeout = 4 * time.hour,
)

ci.gpu.windows_builder(
    name = "Dawn Win10 x64 Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_use_built_dxc",
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Builder",
        short_name = "x64",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.gpu.windows_builder(
    name = "Dawn Win10 x64 DEPS Builder",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_use_built_dxc",
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.gpu.windows_builder(
    name = "Dawn Win11 arm64 Builder",
    description_html = "Compiles ToT binaries for Windows/ARM64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-dawn-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "dawn_use_built_dxc",
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "win",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "dawn_end2end_tests",
            "dawn_perf_tests",
            "gl_tests",
            "gl_unittests",
            "telemetry_gpu_integration_test",
            "telemetry_gpu_unittests",
            "webgpu_blink_web_tests",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Builder",
        short_name = "arm64",
    ),
)

ci.gpu.windows_builder(
    name = "Dawn Win11 arm64 DEPS Builder",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    description_html = "Compiles DEPSed binaries for Windows/ARM64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-dawn-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "dawn_use_built_dxc",
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "gpu_tests",
            "win",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "dawn_end2end_tests",
            "dawn_perf_tests",
            "gl_tests",
            "gl_unittests",
            "telemetry_gpu_integration_test",
            "telemetry_gpu_unittests",
            "webgpu_blink_web_tests",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "arm64",
    ),
)

# Note that the Win testers are all thin Linux VMs, triggering jobs on the
# physical Win hardware in the Swarming pool, which is why they run on linux
ci.thin_tester(
    name = "Dawn Win10 x64 DEPS Release (Intel)",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    triggered_by = ["ci/Dawn Win10 x64 DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_win_x64_tests",
            "gpu_dawn_integration_gtests_passthrough_win_x64",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "win10_intel_uhd_630_stable",
        ],
        per_test_modifications = {
            "webgpu_blink_web_tests_with_backend_validation": targets.remove(
                reason = "Remove from bots where capacity is constrained.",
            ),
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_with_validation_tests": targets.remove(
                reason = "Remove from bots where capacity is constrained.",
            ),
            "webgpu_swiftshader_web_platform_cts_with_validation_tests": targets.remove(
                reason = "Remove from bots where capacity is constrained.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|x64|Intel",
        short_name = "630",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Dawn Win10 x64 DEPS Release (NVIDIA)",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    triggered_by = ["ci/Dawn Win10 x64 DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_win_x64_tests",
            "gpu_dawn_integration_gtests_passthrough_win_x64",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|x64|Nvidia",
        short_name = "1660",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Dawn Win10 x64 Experimental Release (Intel)",
    triggered_by = ["Dawn Win10 x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental driver is identical to the stable driver, this
        # should be running the gpu_noop_sleep_telemetry_test. Otherwise, it
        # should be running the same test_suites as
        # 'Dawn Win10 x64 Release (Intel)'
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "limited_capacity_bot",
            "win10_intel_uhd_630_experimental",
        ],
        per_test_modifications = {
            # "webgpu_blink_web_tests_with_backend_validation": targets.remove(
            #     reason = "Remove from bots where capacity is constrained.",
            # ),
            # "webgpu_cts_dedicated_worker_tests": targets.remove(
            #     reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            # ),
            # "webgpu_cts_service_worker_tests": targets.remove(
            #     reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            # ),
            # "webgpu_cts_shared_worker_tests": targets.remove(
            #     reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            # ),
            # "webgpu_cts_with_validation_tests": targets.remove(
            #     reason = "Remove from bots where capacity is constrained.",
            # ),
            # "webgpu_swiftshader_web_platform_cts_with_validation_tests": targets.remove(
            #     reason = "Remove from bots where capacity is constrained.",
            # ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "ToT|Windows|x64|Intel",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Dawn Win10 x64 Release (Intel)",
    triggered_by = ["Dawn Win10 x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_win_x64_tests",
            "gpu_dawn_integration_gtests_passthrough_win_x64",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "win10_intel_uhd_630_stable",
        ],
        per_test_modifications = {
            "webgpu_blink_web_tests_with_backend_validation": targets.remove(
                reason = "Remove from bots where capacity is constrained.",
            ),
            "webgpu_cts_dedicated_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_service_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_shared_worker_tests": targets.remove(
                reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            ),
            "webgpu_cts_with_validation_tests": targets.remove(
                reason = "Remove from bots where capacity is constrained.",
            ),
            "webgpu_swiftshader_web_platform_cts_with_validation_tests": targets.remove(
                reason = "Remove from bots where capacity is constrained.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|x64|Intel",
        short_name = "630",
    ),
)

ci.thin_tester(
    name = "Dawn Win10 x64 Release (Intel UHD 770)",
    description_html = "Runs ToT Dawn tests on 12th gen Intel CPUs with UHD 770 GPUs",
    triggered_by = ["Dawn Win10 x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_win_x64_tests",
            "gpu_dawn_integration_gtests_passthrough_win_x64",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "win10_intel_uhd_770_stable",
        ],
        per_test_modifications = {
            "gl_tests_passthrough": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/win.uhd_770.gl_tests_passthrough.filter",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|x64|Intel",
        short_name = "770",
    ),
)

ci.thin_tester(
    name = "Dawn Win10 x64 Experimental Release (NVIDIA)",
    description_html = "Runs ToT Dawn tests on experimental NVIDIA configs",
    triggered_by = ["Dawn Win10 x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental driver is identical to the stable driver, this
        # should be running the gpu_noop_sleep_telemetry_test. Otherwise, it
        # should be running the same test_suites as
        # 'Dawn Win10 x64 Release (NVIDIA)'
        targets = [
            "gpu_dawn_telemetry_win_x64_tests",
            "gpu_dawn_integration_gtests_passthrough_win_x64",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "limited_capacity_bot",
            "win10_nvidia_gtx_1660_experimental",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|x64|Nvidia",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
    execution_timeout = 6 * time.hour,
)

ci.thin_tester(
    name = "Dawn Win10 x64 Release (NVIDIA)",
    triggered_by = ["Dawn Win10 x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_win_x64_tests",
            "gpu_dawn_integration_gtests_passthrough_win_x64",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            # TODO(crbug.com/40888390): The swarming dimensions for
            # webgpu_blink_web_tests and webgpu_cts_tests on
            # win10-code-coverage, in test_suite_exceptions.pyl, must be kept
            # manually in sync with this configuration.
            "win10_nvidia_gtx_1660_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE_X64,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|x64|Nvidia",
        short_name = "1660",
    ),
)

ci.gpu.windows_builder(
    name = "Dawn Win10 x86 Builder",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "dawn_top_of_tree",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "x86",
            "gpu_tests",
            "win",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Builder",
        short_name = "x86",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.gpu.windows_builder(
    name = "Dawn Win10 x86 DEPS Builder",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "x86",
            "gpu_tests",
            "win",
        ],
    ),
    targets = targets.bundle(),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x86",
    ),
    cq_mirrors_console_view = "mirrors",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

# Note that the Win testers are all thin Linux VMs, triggering jobs on the
# physical Win hardware in the Swarming pool, which is why they run on linux
ci.thin_tester(
    name = "Dawn Win10 x86 DEPS Release (Intel)",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    triggered_by = ["ci/Dawn Win10 x86 DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_tests_fxc",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "win10_intel_uhd_630_stable",
        ],
        per_test_modifications = {
            "webgpu_cts_fxc_with_validation_tests": targets.remove(
                reason = [
                    "Disable testing with validation on x86 where they frequently OOM.",
                    "See crbug.com/1444815.",
                ],
            ),
            "webgpu_swiftshader_web_platform_cts_with_validation_tests": targets.remove(
                reason = "Remove from bots where capacity is constrained.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|x86|Intel",
        short_name = "630",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Dawn Win10 x86 DEPS Release (NVIDIA)",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    triggered_by = ["ci/Dawn Win10 x86 DEPS Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_tests_fxc",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
        per_test_modifications = {
            "webgpu_cts_fxc_with_validation_tests": targets.remove(
                reason = [
                    "Disable testing with validation on x86 where they frequently OOM.",
                    "See crbug.com/1444815.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|x86|Nvidia",
        short_name = "1660",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Dawn Win10 x86 Experimental Release (Intel)",
    triggered_by = ["Dawn Win10 x86 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental driver is identical to the stable driver, this
        # should be running the gpu_noop_sleep_telemetry_test. Otherwise, it
        # should be running the same test_suites as
        # 'Dawn Win10 x86 Release (Intel)'
        targets = [
            "gpu_noop_sleep_telemetry_test",
        ],
        mixins = [
            "limited_capacity_bot",
            "win10_intel_uhd_630_experimental",
        ],
        per_test_modifications = {
            # "webgpu_cts_dedicated_worker_tests": targets.remove(
            #     reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            # ),
            # "webgpu_cts_fxc_with_validation_tests": targets.remove(
            #     reason = [
            #         "Disable testing with validation on x86 where they frequently OOM.",
            #         "See crbug.com/1444815.",
            #     ],
            # ),
            # "webgpu_cts_service_worker_tests": targets.remove(
            #     reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            # ),
            # "webgpu_cts_shared_worker_tests": targets.remove(
            #     reason = "We only need coverage on one GPU per OS, so remove from lower capacity configs.",
            # ),
            # "webgpu_swiftshader_web_platform_cts_with_validation_tests": targets.remove(
            #     reason = "Remove from bots where capacity is constrained.",
            # ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.WINDOWS,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "ToT|Windows|x86|Intel",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Dawn Win10 x86 Experimental Release (NVIDIA)",
    description_html = "Runs ToT Dawn tests on experimental Win/NVIDIA/x86 configs",
    triggered_by = ["Dawn Win10 x86 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        # When the experimental driver is identical to the stable driver, this
        # should be running the gpu_noop_sleep_telemetry_test. Otherwise, it
        # should be running the same test_suites as 'Dawn Win10 x86 Release
        # (NVIDIA)'.
        targets = [
            "gpu_dawn_telemetry_tests_fxc",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "limited_capacity_bot",
            "win10_nvidia_gtx_1660_experimental",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.WINDOWS,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|x86|Nvidia",
        short_name = "exp",
    ),
    list_view = "chromium.gpu.experimental",
)

ci.thin_tester(
    name = "Dawn Win10 x86 Release (Intel)",
    triggered_by = ["Dawn Win10 x86 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_tests_fxc",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "win10_intel_uhd_630_stable",
        ],
        per_test_modifications = {
            "webgpu_cts_fxc_with_validation_tests": targets.remove(
                reason = [
                    "Disable testing with validation on x86 where they frequently OOM.",
                    "See crbug.com/1444815.",
                ],
            ),
            "webgpu_swiftshader_web_platform_cts_with_validation_tests": targets.remove(
                reason = "Remove from bots where capacity is constrained.",
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|x86|Intel",
        short_name = "630",
    ),
)

ci.thin_tester(
    name = "Dawn Win10 x86 Release (NVIDIA)",
    triggered_by = ["Dawn Win10 x86 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
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
        build_gs_bucket = "chromium-dawn-archive",
        run_tests_serially = True,
    ),
    targets = targets.bundle(
        targets = [
            "gpu_dawn_telemetry_tests_fxc",
            "gpu_dawn_integration_gtests_passthrough",
            "gpu_dawn_isolated_scripts",
        ],
        mixins = [
            "win10_nvidia_gtx_1660_stable",
        ],
        per_test_modifications = {
            "webgpu_cts_fxc_with_validation_tests": targets.remove(
                reason = [
                    "Disable testing with validation on x86 where they frequently OOM.",
                    "See crbug.com/1444815.",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|x86|Nvidia",
        short_name = "1660",
    ),
)
