# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.win builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.win",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.WINDOWS_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,
    main_console_view = "main",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.win",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    ordering = {
        None: ["release", "debug"],
        "debug|builder": consoles.ordering(short_names = ["64", "32"]),
        "debug|tester": consoles.ordering(short_names = ["7", "10"]),
    },
)

# TODO(gbeaty) Investigate if the testers need to run on windows, if not, switch
# them to ci.thin_tester

ci.builder(
    name = "WebKit Win10",
    triggered_by = ["Win Builder"],
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
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "misc",
        short_name = "wbk",
    ),
)

ci.builder(
    name = "Win Builder",
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
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "release|builder",
        short_name = "32",
    ),
)

ci.builder(
    name = "Win x64 Builder (dbg)",
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
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    builderless = True,
    cores = 32,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
)

ci.builder(
    name = "Win10 Tests x64 (dbg)",
    triggered_by = ["Win x64 Builder (dbg)"],
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
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    # Too flaky. See crbug.com/876224 for more details.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "debug|tester",
        short_name = "10",
    ),
)

ci.builder(
    name = "Win Builder (dbg)",
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "32",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.builder(
    name = "Win x64 Builder",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
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
        build_gs_bucket = "chromium-win-archive",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "release|builder",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.builder(
    name = "Win10 Tests x64",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    triggered_by = ["ci/Win x64 Builder"],
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
        build_gs_bucket = "chromium-win-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "w10",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Win11 Tests x64",
    triggered_by = ["ci/Win x64 Builder"],
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
        build_gs_bucket = "chromium-win-archive",
    ),
    # TODO(kuanhuang): Add back to sheriff rotation after verified green.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "w11",
    ),
)

ci.builder(
    name = "Windows deterministic",
    executable = "recipe:swarming/deterministic_build",
    console_view_entry = consoles.console_view_entry(
        category = "misc",
        short_name = "det",
    ),
    execution_timeout = 12 * time.hour,
    reclient_bootstrap_env = {
        "RBE_ip_timeout": "10m",
    },
)
