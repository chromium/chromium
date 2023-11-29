# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.dawn builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "reclient", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.dawn",
    pool = ci.gpu.POOL,
    sheriff_rotations = sheriff_rotations.DAWN,
    contact_team_email = "chrome-gpu-infra@google.com",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.gpu.SERVICE_ACCOUNT,
    shadow_service_account = ci.gpu.SHADOW_SERVICE_ACCOUNT,
    thin_tester_cores = 2,
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Builder",
        short_name = "x64",
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
        ],
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
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Linux|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
        ],
    ),
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
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Android|Builder",
        short_name = "arm",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
        ],
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Android",
        short_name = "p4",
    ),
)

ci.gpu.linux_builder(
    name = "Dawn Android arm64 DEPS Release (Pixel 6)",
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
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Android",
        short_name = "p6",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "arm64",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
        ],
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|TSAN",
        short_name = "x64",
    ),
    # Serially executed tests + TSAN = more than the default timeout needed in
    # order to prevent build timeouts.
    execution_timeout = 6 * time.hour,
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "tsan",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
        ],
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Android|Builder",
        short_name = "arm",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
        ],
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Android",
        short_name = "p4",
    ),
)

ci.gpu.linux_builder(
    name = "Dawn Android arm64 Release (Pixel 6)",
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
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Android",
        short_name = "p6",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "arm64",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
        ],
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Intel",
        short_name = "x64",
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Linux|Nvidia",
        short_name = "x64",
    ),
)

ci.gpu.mac_builder(
    name = "Dawn Mac arm64 DEPS Release (Apple M2)",
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
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Mac",
        short_name = "arm64",
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
            "arm64",
        ],
    ),
)

ci.gpu.mac_builder(
    name = "Dawn Mac arm64 Release (Apple M2)",
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
        run_tests_serially = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac",
        short_name = "arm64",
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
            "arm64",
        ],
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Mac|Builder",
        short_name = "x64",
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
            "x64",
        ],
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
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Mac|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
            "x64",
        ],
    ),
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
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "ToT|Mac|AMD",
    #     short_name = "exp",
    # ),
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
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "ToT|Mac|Intel",
    #     short_name = "exp",
    # ),
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|ASAN|Builder",
        short_name = "x64",
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_use_built_dxc",
            "dawn_enable_opengles",
            "asan",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
        ],
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|ASAN|Intel",
        short_name = "x64",
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|ASAN|Nvidia",
        short_name = "x64",
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Builder",
        short_name = "x64",
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_use_built_dxc",
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
        ],
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    gn_args = gn_args.config(
        configs = [
            "dawn_use_built_dxc",
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
        ],
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Intel",
        short_name = "x64",
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
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Nvidia",
        short_name = "x64",
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
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "ToT|Windows|Intel",
    #     short_name = "ex64",
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Intel",
        short_name = "x64",
    ),
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Nvidia",
        short_name = "x64",
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Builder",
        short_name = "x86",
    ),
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
            "x86",
        ],
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Builder",
        short_name = "x86",
    ),
    cq_mirrors_console_view = "mirrors",
    gn_args = gn_args.config(
        configs = [
            "dawn_enable_opengles",
            "release_try_builder",
            "minimal_symbols",
            "reclient",
            "x86",
        ],
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Intel",
        short_name = "x86",
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
    console_view_entry = consoles.console_view_entry(
        category = "DEPS|Windows|Nvidia",
        short_name = "x86",
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
    # Uncomment this entry when this experimental tester is actually in use.
    # console_view_entry = consoles.console_view_entry(
    #     category = "ToT|Windows|Intel",
    #     short_name = "ex86",
    # ),
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Intel",
        short_name = "x86",
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
    console_view_entry = consoles.console_view_entry(
        category = "ToT|Windows|Nvidia",
        short_name = "x86",
    ),
)
