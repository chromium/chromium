# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.gpu builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "reclient", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/structs.star", "structs")

ci.defaults.set(
    builder_group = "chromium.gpu",
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    pool = ci.gpu.POOL,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM_GPU,
    tree_closing = True,
    tree_closing_notifiers = ci.gpu.TREE_CLOSING_NOTIFIERS,
    thin_tester_cores = 2,
)

consoles.console_view(
    name = "chromium.gpu",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    ordering = {
        None: ["Windows", "Mac", "Linux"],
    },
)

ci.gpu.linux_builder(
    name = "Android Release (Nexus 5X)",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "download_vr_test_apks",
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
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "Android",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.linux_builder(
    name = "GPU Linux Builder",
    branch_selector = branches.STANDARD_MILESTONE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.gpu.linux_builder(
    name = "GPU Linux Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    tree_closing = False,
    sheriff_rotations = args.ignore_default(None),
)

ci.gpu.mac_builder(
    name = "GPU Mac Builder",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    builder_spec = builder_config.builder_spec(
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
                "goma_use_local",  # to mitigate compile step timeout (crbug.com/1056935)
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
    goma_backend = goma.backend.RBE_PROD,
    reclient_instance = None,
)

ci.gpu.mac_builder(
    name = "GPU Mac Builder (reclient shadow)",
    builder_spec = builder_config.copy_from(
        "ci/GPU Mac Builder",
        lambda spec: structs.evolve(
            spec,
            build_gs_bucket = None,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
        short_name = "rec",
    ),
    goma_backend = None,
    tree_closing = False,
    sheriff_rotations = args.ignore_default(None),
)

ci.gpu.mac_builder(
    name = "GPU Mac Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    goma_backend = goma.backend.RBE_PROD,
    reclient_instance = None,
)

ci.gpu.windows_builder(
    name = "GPU Win x64 Builder",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    builder_spec = builder_config.builder_spec(
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
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.gpu.windows_builder(
    name = "GPU Win x64 Builder (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "Linux Debug (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    sheriff_rotations = args.ignore_default(None),
    triggered_by = ["GPU Linux Builder (dbg)"],
    tree_closing = False,
)

ci.thin_tester(
    name = "Linux Release (NVIDIA)",
    branch_selector = branches.STANDARD_MILESTONE,
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
        ),
        build_gs_bucket = "chromium-gpu-archive",
    ),
    cq_mirrors_console_view = "mirrors",
    console_view_entry = consoles.console_view_entry(
        category = "Linux",
    ),
    triggered_by = ["ci/GPU Linux Builder"],
)

ci.thin_tester(
    name = "Mac Debug (Intel)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    sheriff_rotations = args.ignore_default(None),
    triggered_by = ["GPU Mac Builder (dbg)"],
    tree_closing = False,
)

ci.thin_tester(
    name = "Mac Release (Intel)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "goma_use_local",  # to mitigate compile step timeout (crbug.com/1056935)
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
    triggered_by = ["ci/GPU Mac Builder"],
)

ci.thin_tester(
    name = "Mac Retina Debug (AMD)",
    console_view_entry = consoles.console_view_entry(
        category = "Mac",
    ),
    triggered_by = ["GPU Mac Builder (dbg)"],
    tree_closing = False,
)

ci.thin_tester(
    name = "Mac Retina Release (AMD)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "goma_use_local",  # to mitigate compile step timeout (crbug.com/1056935)
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
    triggered_by = ["ci/GPU Mac Builder"],
)

ci.thin_tester(
    name = "Win10 x64 Debug (NVIDIA)",
    console_view_entry = consoles.console_view_entry(
        category = "Windows",
    ),
    triggered_by = ["GPU Win x64 Builder (dbg)"],
    tree_closing = False,
)

ci.thin_tester(
    name = "Win10 x64 Release (NVIDIA)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
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
    triggered_by = ["ci/GPU Win x64 Builder"],
)
