# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.swangle builder group."""

load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/gpu.star", "gpu")
load("//lib/siso.star", "siso")

ci.defaults.set(
    executable = "recipe:angle_chromium",
    builder_group = "chromium.swangle",
    pool = gpu.ci.POOL,
    gardener_rotations = gardener_rotations.CHROMIUM_GPU,
    contact_team_email = "chrome-gpu-infra@google.com",
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    service_account = gpu.ci.SERVICE_ACCOUNT,
    shadow_service_account = gpu.ci.SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
)

consoles.console_view(
    name = "chromium.swangle",
    ordering = {
        None: ["DEPS", "ToT SwiftShader", "Chromium"],
        "*os*": ["Windows", "Mac"],
        "*cpu*": consoles.ordering(short_names = ["x86", "x64"]),
        "DEPS": "*os*",
        "DEPS|Windows": "*cpu*",
        "DEPS|Linux": "*cpu*",
        "ToT SwiftShader": "*os*",
        "ToT SwiftShader|Windows": "*cpu*",
        "ToT SwiftShader|Linux": "*cpu*",
        "Chromium": "*os*",
    },
)

gpu.ci.linux_builder(
    name = "linux-swangle-chromium-x64",
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
                "swiftshader_top_of_tree",
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
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "gpu_swangle_telemetry_tests",
        ],
        mixins = [
            "gpu-swarming-pool",
            "isolate_profile_data",
            "linux-jammy",
            "no_gpu",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Chromium|Linux",
        short_name = "x64",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

gpu.ci.linux_builder(
    name = "linux-swangle-chromium-x64-exp",
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
                "swiftshader_top_of_tree",
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
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        mixins = [
            "gpu-swarming-pool",
            "isolate_profile_data",
            "linux-jammy",
            "no_gpu",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.LINUX,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "Chromium|Linux",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

gpu.ci.linux_builder(
    name = "linux-swangle-tot-swiftshader-x64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "swiftshader_top_of_tree",
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
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "shared",
            "release",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "swangle_gtests",
        ],
        mixins = [
            "gpu-swarming-pool",
            "isolate_profile_data",
            "linux-jammy",
            "no_gpu",
            "timeout_15m",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT SwiftShader|Linux",
        short_name = "x64",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

gpu.ci.linux_builder(
    name = "linux-swangle-x64",
    executable = ci_constants.DEFAULT_EXECUTABLE,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "shared",
            "release",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "swangle_gtests",
        ],
        mixins = [
            "gpu-swarming-pool",
            "isolate_profile_data",
            "linux-jammy",
            "no_gpu",
            "timeout_15m",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.LINUX,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Linux",
        short_name = "x64",
    ),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

gpu.ci.linux_builder(
    name = "linux-swangle-x64-exp",
    executable = ci_constants.DEFAULT_EXECUTABLE,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "shared",
            "release",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        mixins = [
            "gpu-swarming-pool",
            "isolate_profile_data",
            "linux-jammy",
            "no_gpu",
            "timeout_15m",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.LINUX,
    ),
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "DEPS|Linux",
    #     short_name = "exp",
    # ),
    list_view = "chromium.gpu.experimental",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

gpu.ci.mac_builder(
    name = "mac-swangle-chromium-x64",
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
                "swiftshader_top_of_tree",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "gpu_swangle_telemetry_tests",
        ],
        mixins = [
            "mac_mini_intel_gpu_stable",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.MAC,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Chromium|Mac",
        short_name = "x64",
    ),
)

gpu.ci.windows_builder(
    name = "win-swangle-chromium-x86",
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_top_of_tree",
                "swiftshader_top_of_tree",
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
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_try_builder",
            "minimal_symbols",
            "remoteexec",
            "x86",
            "resource_allowlisting",
            "win",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "gpu_swangle_telemetry_tests",
        ],
        mixins = [
            "win10_gce_gpu_pool",
        ],
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.RELEASE,
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Chromium|Windows",
        short_name = "x86",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

gpu.ci.windows_builder(
    name = "win-swangle-tot-swiftshader-x64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "swiftshader_top_of_tree",
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
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "shared",
            "release",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "swangle_gtests",
        ],
        mixins = [
            "win10_gce_gpu_pool",
            "timeout_15m",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT SwiftShader|Windows",
        short_name = "x64",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

gpu.ci.windows_builder(
    name = "win-swangle-tot-swiftshader-x86",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "swiftshader_top_of_tree",
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
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "shared",
            "release",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "win",
            "x86",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "swangle_gtests",
        ],
        mixins = [
            "win10_gce_gpu_pool",
            "timeout_15m",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT SwiftShader|Windows",
        short_name = "x86",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

gpu.ci.windows_builder(
    name = "win-swangle-x64",
    executable = ci_constants.DEFAULT_EXECUTABLE,
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
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "shared",
            "release",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "swangle_gtests",
        ],
        mixins = [
            "win10_gce_gpu_pool",
            "timeout_15m",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows",
        short_name = "x64",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

gpu.ci.windows_builder(
    name = "win-swangle-x86",
    executable = ci_constants.DEFAULT_EXECUTABLE,
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
        run_tests_serially = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "angle_deqp_tests",
            "shared",
            "release",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "win",
            "x86",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "swangle_gtests",
        ],
        mixins = [
            "win10_gce_gpu_pool",
            "timeout_15m",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.WINDOWS,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows",
        short_name = "x86",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)
