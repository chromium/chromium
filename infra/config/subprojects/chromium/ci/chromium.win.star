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
    builder_group = "chromium.win",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    main_console_view = "main",
    os = os.WINDOWS_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,
)

consoles.console_view(
    name = "chromium.win",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
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
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "goma_enable_global_file_stat_cache",
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
    triggered_by = ["Win Builder"],
)

ci.builder(
    name = "Win Builder",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "goma_enable_global_file_stat_cache",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|builder",
        short_name = "32",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.builder(
    name = "Win10 Tests x64 (dbg)",
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|tester",
        short_name = "10",
    ),
    triggered_by = ["Win x64 Builder (dbg)"],
    # Too flaky. See crbug.com/876224 for more details.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
)

ci.thin_tester(
    name = "Win7 (32) Tests",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "goma_enable_global_file_stat_cache",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "32",
    ),
    triggered_by = ["Win Builder"],
)

ci.builder(
    name = "Win7 Tests (1)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "goma_enable_global_file_stat_cache",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "32",
    ),
    os = os.WINDOWS_10,
    triggered_by = ["Win Builder"],
)

ci.builder(
    name = "Win 7 Tests x64 (1)",
    builderless = True,
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
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    os = os.WINDOWS_10,
    triggered_by = ["ci/Win x64 Builder"],
)

ci.builder(
    name = "Win Builder (dbg)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "32",
    ),
    cores = 32,
    cq_mirrors_console_view = "mirrors",
    os = os.WINDOWS_ANY,
)

ci.builder(
    name = "Win x64 Builder",
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
        build_gs_bucket = "chromium-win-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|builder",
        short_name = "64",
    ),
    cores = 32,
    cq_mirrors_console_view = "mirrors",
    os = os.WINDOWS_ANY,
)

ci.builder(
    name = "Win10 Tests x64",
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
        build_gs_bucket = "chromium-win-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "w10",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Win x64 Builder"],
)

ci.thin_tester(
    name = "Win11 Tests x64",
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
        short_name = "w11",
    ),
    triggered_by = ["ci/Win x64 Builder"],
    # TODO(kuanhuang): Add back to sheriff rotation after verified green.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
)

ci.builder(
    name = "Windows deterministic",
    console_view_entry = consoles.console_view_entry(
        category = "misc",
        short_name = "det",
    ),
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 12 * time.hour,
)
