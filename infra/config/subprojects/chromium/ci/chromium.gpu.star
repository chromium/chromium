# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.gpu builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "gardener_rotations", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.gpu",
    pool = ci.gpu.POOL,
    tree_closing = True,
    contact_team_email = "chrome-gpu-infra@google.com",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    gardener_rotations = gardener_rotations.CHROMIUM_GPU,
    health_spec = health_spec.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
    thin_tester_cores = 2,
    tree_closing_notifiers = ci.gpu.TREE_CLOSING_NOTIFIERS,
)

consoles.console_view(
    name = "chromium.gpu",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
        branches.selector.DESKTOP_BRANCHES,
    ],
    ordering = {
        None: ["Windows", "Mac", "Linux"],
    },
)

ci.gpu.linux_builder(
    name = "Android Release (Nexus 5X)",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_xr_test_apks",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "android_builder",
            "release_builder",
            "try_builder",
            "remoteexec",
            "arm64",
            "static_angle",
            "android_fastbuild",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Android",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.linux_builder(
    name = "GPU Linux Builder",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
                # This is not necessary for this builder itself, but is
                # necessary in order to keep configs in sync with
                # "ci/Linux Builder" in order for mirroring to work correctly.
                "chromium_with_telemetry_dependencies",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.linux_builder(
    name = "GPU Linux Builder (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "remoteexec",
        ],
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    gardener_rotations = args.ignore_default(None),
)

ci.gpu.mac_builder(
    name = "GPU Mac Builder",
    branch_selector = branches.selector.MAC_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is not necessary for this builder itself, but is
                # necessary in order to keep configs in sync with
                # "ci/Mac Builder" in order for mirroring to work correctly.
                "chromium_with_telemetry_dependencies",
                "use_clang_coverage",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.mac_builder(
    name = "GPU Mac Builder (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "remoteexec",
            "x64",
        ],
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    gardener_rotations = args.ignore_default(None),
)

ci.gpu.windows_builder(
    name = "GPU Win x64 Builder",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is not necessary for this builder itself, but is
                # necessary in order to keep configs in sync with
                # "ci/Mac Builder" in order for mirroring to work correctly.
                "chromium_with_telemetry_dependencies",
                "use_clang_coverage",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "try_builder",
            "remoteexec",
            "resource_allowlisting",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    cq_mirrors_console_view = "mirrors",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.gpu.windows_builder(
    name = "GPU Win x64 Builder (dbg)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "remoteexec",
        ],
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    gardener_rotations = args.ignore_default(None),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "Linux Debug (NVIDIA)",
    triggered_by = ["GPU Linux Builder (dbg)"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    gardener_rotations = args.ignore_default(None),
)

ci.thin_tester(
    name = "Linux Release (NVIDIA)",
    branch_selector = branches.selector.LINUX_BRANCHES,
    triggered_by = ["ci/GPU Linux Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Mac Debug (Intel)",
    triggered_by = ["GPU Mac Builder (dbg)"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    gardener_rotations = args.ignore_default(None),
)

ci.thin_tester(
    name = "Mac Release (Intel)",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/GPU Mac Builder"],
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Mac Retina Debug (AMD)",
    triggered_by = ["GPU Mac Builder (dbg)"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
)

ci.thin_tester(
    name = "Mac Retina Release (AMD)",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/GPU Mac Builder"],
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
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Win10 x64 Debug (NVIDIA)",
    triggered_by = ["GPU Win x64 Builder (dbg)"],
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
)

ci.thin_tester(
    name = "Win10 x64 Release (NVIDIA)",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    triggered_by = ["ci/GPU Win x64 Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
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
        build_gs_bucket = "chromium-gpu-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    cq_mirrors_console_view = "mirrors",
)
