# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.win builder group."""

load("@chromium-luci//args.star", "args")
load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify_builder")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/siso.star", "siso")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.win",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    pool = ci_constants.DEFAULT_POOL,
    cores = 8,
    os = os.WINDOWS_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    tree_closing = True,
    tree_closing_notifiers = ci_constants.DEFAULT_TREE_CLOSING_NOTIFIERS,
    main_console_view = "main",
    contact_team_email = "chrome-desktop-engprod@google.com",
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.default(),
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
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
    parent = "Win Builder",
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
        build_gs_bucket = "chromium-win-archive",
    ),
    targets = targets.bundle(
        targets = [
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "win10",
        ],
    ),
    builderless = False,
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
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "x86",
            "no_symbols",
            "win",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_win_scripts",
        ],
        additional_compile_targets = [
            "ipc_fuzzer",
            "pdf_fuzzers",
        ],
    ),
    builderless = False,
    cores = 16,
    os = os.WINDOWS_ANY,
    ssd = True,
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
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    cores = 16,
    os = os.WINDOWS_ANY,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
)

ci.builder(
    name = "Win10 Tests x64 (dbg)",
    parent = "Win x64 Builder (dbg)",
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
        build_gs_bucket = "chromium-win-archive",
    ),
    targets = targets.bundle(
        targets = [
            "chromium_win_gtests",
            "chromium_win_dbg_isolated_scripts",
        ],
        mixins = [
            "x86-64",
            "win10",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.remove(
                reason = "Not enabled on dbg due to resource limits.",
            ),
            "blink_wpt_tests": targets.remove(
                reason = "Not enabled on dbg due to resource limits.",
            ),
            "chrome_wpt_tests": targets.remove(
                reason = "Not enabled on dbg due to resource limits.",
            ),
            "headless_shell_wpt_tests": targets.remove(
                reason = "Not enabled on dbg due to resource limits.",
            ),
            "browser_tests": targets.remove(
                reason = "Disabled due to failing test suites (crbug/40565753)",
            ),
            "chromedriver_py_tests": targets.remove(
                reason = "Timeout happens sometimes (crbug.com/951799)",
            ),
            "components_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "content_shell_crash_test": targets.mixin(
                # https://crbug.com/861730
                experiment_percentage = 100,
            ),
            "extensions_browsertests": targets.mixin(
                # https://crbug.com/876615
                experiment_percentage = 100,
            ),
            "interactive_ui_tests": targets.mixin(
                # temporary, https://crbug.com/818832
                experiment_percentage = 100,
            ),
            "leveldb_unittests": targets.mixin(
                args = [
                    "--test-launcher-timeout=90000",
                ],
            ),
            "performance_test_suite": targets.mixin(
                args = [
                    "--browser=debug_x64",
                ],
                experiment_percentage = 100,
            ),
            "sync_integration_tests": targets.mixin(
                # https://crbug.com/840369
                experiment_percentage = 100,
            ),
            "telemetry_perf_unittests": targets.remove(
                reason = "Disabled due to failing test suites (crbug/40565753)",
            ),
            "telemetry_unittests": targets.mixin(
                # crbug.com/870673
                experiment_percentage = 100,
            ),
        },
    ),
    # Too flaky. See crbug.com/876224 for more details.
    gardener_rotations = args.ignore_default(None),
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
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "remoteexec",
            "x86",
            "no_symbols",
            "win",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    builderless = False,
    cores = 16,
    os = os.WINDOWS_ANY,
    ssd = True,
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
                # This is necessary due to child builders running the
                # telemetry_perf_unittests suite.
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
        build_gs_bucket = "chromium-win-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_win_scripts",
        ],
        additional_compile_targets = [
            "base_nocompile_tests",
            "blink_platform_nocompile_tests",
            "blink_probes_nocompile_tests",
            "content_nocompile_tests",
            "ipc_fuzzer",
            "pdf_fuzzers",
        ],
    ),
    builderless = False,
    cores = 16,
    os = os.WINDOWS_ANY,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "release|builder",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.builder(
    name = "Win10 Tests x64",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    parent = "ci/Win x64 Builder",
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
    targets = targets.bundle(
        targets = [
            "chromium_win10_gtests",
            "chromium_win_rel_isolated_scripts_once",
            "gtests_once",
        ],
        mixins = [
            "x86-64",
            "win10",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    # blink_web_tests has issues when mixing different CPUs.
                    # see https://crbug.com/1458859
                    # As of 2024 Q4, all e2 machines in chromium.tests use
                    # x86-64-Broadwell_GCE. But, the situation may change when
                    # GCE replaces the hardwares. If that happens, it needs to
                    # be updated to run on the most popular CPU platform.
                    dimensions = {
                        "cpu": "x86-64-Broadwell_GCE",
                    },
                    shards = 12,
                ),
            ),
            "browser_tests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1473501
                retry_only_failed_tests = True,
                swarming = targets.swarming(
                    # This is for slow test execution that often becomes a
                    # critical path of swarming jobs. crbug.com/868114
                    shards = 55,
                ),
            ),
            "chromedriver_py_tests": targets.mixin(
                # TODO(crbug.com/40868908): Fix & re-enable.
                isolate_profile_data = False,
            ),
            "content_browsertests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1475852
                retry_only_failed_tests = True,
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
            "interactive_ui_tests_no_field_trial": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "sync_integration_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 3,
                ),
            ),
            "telemetry_perf_unittests": targets.remove(
                reason = "Some test cases fail on win-rel (crbug/40622135).",
            ),
            "telemetry_unittests": targets.remove(
                reason = "Some test cases fail on win-rel (crbug/40622135).",
            ),
        },
    ),
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "w10",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Win10 Tests x86",
    description_html = "Windows x86 release build running on x64 testing bots.",
    parent = "ci/Win Builder",
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
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    targets = targets.bundle(
        targets = [
            "win_x86_specific_smoke_tests",
        ],
        mixins = [
            "x86-64",
            "win10",
            "isolate_profile_data",
        ],
    ),
    builderless = True,
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "misc",
        short_name = "x86",
    ),
)

ci.thin_tester(
    name = "Win11 Tests x64",
    parent = "ci/Win x64 Builder",
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
    targets = targets.bundle(
        targets = [
            "chromium_win10_gtests",
            "chromium_win_rel_isolated_scripts",
        ],
        mixins = [
            "x86-64",
            "win11",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    # This is for slow test execution that often becomes a
                    # critical path of swarming jobs. crbug.com/868114
                    shards = 15,
                ),
            ),
            "browser_tests_no_field_trial": targets.remove(
                reason = "crbug/40630866",
            ),
            "components_browsertests_no_field_trial": targets.remove(
                reason = "crbug/40630866",
            ),
            "interactive_ui_tests_no_field_trial": targets.remove(
                reason = "crbug/40630866",
            ),
            "pixel_browser_tests": targets.remove(
                reason = [
                    "This target should be removed from any CI only builders.",
                    "Developers can intentionally make UI changes. Without ",
                    "running pixel tests on CQ, those cls will get wrongly ",
                    "reverted by sheriffs.",
                    "When we switch CQ builders(e.g. use Win11 to replace ",
                    "Win10), we also need to update this field.",
                ],
            ),
            "pixel_interactive_ui_tests": targets.remove(
                reason = [
                    "This target should be removed from any CI only builders.",
                    "Developers can intentionally make UI changes. Without ",
                    "running pixel tests on CQ, those cls will get wrongly ",
                    "reverted by sheriffs.",
                    "When we switch CQ builders(e.g. use Win11 to replace ",
                    "Win10), we also need to update this field.",
                ],
            ),
            "sync_integration_tests_no_field_trial": targets.remove(
                reason = "crbug/40630866",
            ),
            "telemetry_perf_unittests": targets.remove(
                reason = "Similar to Win10 Tests x64. Some test cases fail on win-rel (crbug/40622135)",
            ),
            "telemetry_unittests": targets.remove(
                reason = "Similar to Win10 Tests x64. Some test cases fail on win-rel (crbug/40622135)",
            ),
        },
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "w11",
    ),
)

ci.builder(
    name = "win-arm64-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    description_html = "Windows ARM64 Release Builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                # This is necessary due to child builders running the
                # telemetry_perf_unittests suite.
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
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "gpu_tests",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "win",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome",
            "chromedriver_group",
        ],
    ),
    builderless = False,
    cores = 16,
    os = os.WINDOWS_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "release|builder",
        short_name = "a64",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    # 20min (bot update) + 3hr (compile time without cache) +
    # 40min (isolate tests) with 1hr buffer
    execution_timeout = 5 * time.hour,
)

ci.thin_tester(
    name = "win11-arm64-rel-tests",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    description_html = "Windows11 ARM64 Release Tester.",
    parent = "ci/win-arm64-rel",
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
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
        retry_invalid_shards = True,
    ),
    targets = targets.bundle(
        targets = [
            "chromium_win10_gtests",
            "chromium_win_rel_isolated_scripts_once",
        ],
        mixins = [
            "win-arm64",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    # This is for slow test execution that often becomes a
                    # critical path of swarming jobs. crbug.com/868114
                    shards = 15,
                ),
            ),
            "browser_tests_no_field_trial": targets.remove(
                reason = "Disabled on similar Windows testers due to crbug/40630866.",
            ),
            "components_browsertests_no_field_trial": targets.remove(
                reason = "Disabled on similar Windows testers due to crbug/40630866.",
            ),
            "interactive_ui_tests_no_field_trial": targets.remove(
                reason = "Disabled on similar Windows testers due to crbug/40630866.",
            ),
            "pixel_browser_tests": targets.remove(
                reason = [
                    "This target should be removed from any CI only builders.",
                    "Developers can intentionally make UI changes. Without ",
                    "running pixel tests on CQ, those cls will get wrongly ",
                    "reverted by sheriffs.",
                    "When we switch CQ builders(e.g. use Win11 to replace ",
                    "Win10), we also need to update this field.",
                ],
            ),
            "pixel_interactive_ui_tests": targets.remove(
                reason = [
                    "This target should be removed from any CI only builders.",
                    "Developers can intentionally make UI changes. Without ",
                    "running pixel tests on CQ, those cls will get wrongly ",
                    "reverted by sheriffs.",
                    "When we switch CQ builders(e.g. use Win11 to replace ",
                    "Win10), we also need to update this field.",
                ],
            ),
            "sync_integration_tests_no_field_trial": targets.remove(
                reason = "Disabled on similar Windows testers due to crbug/40630866.",
            ),
            "telemetry_perf_unittests": targets.remove(
                reason = "Disabled on similar Windows testers due to crbug/40622135.",
            ),
            "telemetry_unittests": targets.remove(
                reason = "Disabled on similar Windows testers due to crbug/40622135.",
            ),
            "webui_resources_tools_python_unittests": targets.remove(
                reason = "Unneeded; only run on non-cross-compiling bots",
            ),
        },
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "a64",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
)

ci.builder(
    name = "win-arm64-dbg",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    description_html = "Windows ARM64 Debug Builder.",
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
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "gpu_tests",
            "debug_builder",
            "remoteexec",
            "win",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    cores = 16,
    os = os.WINDOWS_DEFAULT,
    ssd = True,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "a64",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    execution_timeout = 4 * time.hour,
)

ci.thin_tester(
    name = "win11-arm64-dbg-tests",
    description_html = "Windows11 ARM64 Debug Tester.",
    parent = "ci/win-arm64-dbg",
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
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    targets = targets.bundle(
        targets = [
            "chromium_win_gtests",
            "chromium_win_dbg_isolated_scripts",
        ],
        mixins = [
            "win-arm64",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.remove(
                reason = "Not enabled on dbg due to resource limits.",
            ),
            "blink_wpt_tests": targets.remove(
                reason = "Not enabled on dbg due to resource limits.",
            ),
            "chrome_wpt_tests": targets.remove(
                reason = "Not enabled on dbg due to resource limits.",
            ),
            "headless_shell_wpt_tests": targets.remove(
                reason = "Not enabled on dbg due to resource limits.",
            ),
            "browser_tests": targets.remove(
                reason = "Disabled due to failing test suites (crbug/40565753)",
            ),
            "content_browsertests": targets.mixin(
                swarming = targets.swarming(
                    shards = 16,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
            "leveldb_unittests": targets.mixin(
                args = [
                    "--test-launcher-timeout=90000",
                ],
            ),
            "telemetry_perf_unittests": targets.remove(
                reason = "Disabled on similar Windows testers due to crbug/40622135.",
            ),
            "telemetry_unittests": targets.remove(
                reason = "Disabled on similar Windows testers due to crbug/40622135.",
            ),
        },
    ),
    # TODO(crbug.com/40877793): Enable gardening when stable and green.
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "debug|tester",
        short_name = "a64",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
)

ci.builder(
    name = "Windows deterministic",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "x86",
            "minimal_symbols",
            "win",
        ],
    ),
    builderless = False,
    cores = 32,
    ssd = True,
    console_view_entry = consoles.console_view_entry(
        category = "misc",
        short_name = "det",
    ),
    execution_timeout = 12 * time.hour,
)

ci.builder(
    name = "linux-win-cross-rel",
    description_html = "Linux to Windows cross compile.<br/>" +
                       "It builds with the same GN args with " + linkify_builder("ci", "Win x64 Builder", "chromium") +
                       ", and runs the same test suites with " + linkify_builder("ci", "Win10 Tests x64", "chromium"),
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "win",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
            host_platform = builder_config.host_platform.LINUX,
        ),
        build_gs_bucket = "chromium-win-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/Win x64 Builder",
            "win_cross",
        ],
    ),
    targets = targets.bundle(
        # TODO: crbug.com/346921029 - Support the same test suites with
        # Win10 Tests x64.
        targets = [
            "chromium_win10_gtests",
            "chromium_win_rel_isolated_scripts_once",
            "chromium_win_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "win10",
            "x86-64",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.remove(
                reason = "TODO: crbug.com/346921029 - fix broken tests.",
            ),
            "blink_wpt_tests": targets.remove(
                reason = "TODO: crbug.com/346921029 - fix broken tests.",
            ),
            "grit_python_unittests": targets.remove(
                reason = "TODO: crbug.com/346921029 - fix broken tests.",
            ),
            "interactive_ui_tests": targets.mixin(
                # Shadow Win10 Tests x64
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
            "interactive_ui_tests_no_field_trial": targets.mixin(
                # Shadow Win10 Tests x64
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "metrics_python_tests": targets.remove(
                reason = "TODO: crbug.com/347165944 - Fix missing dirmd.exe on Linux.",
            ),
            "mini_installer_tests": targets.remove(
                reason = [
                    "TODO: crbug.com/346921029: support mini_installer_tests",
                    "The label \"//chrome/test/mini_installer:mini_installer_tests(//build/toolchain/win:win_clang_x64)\" ",
                    "isn't a target.",
                    "https://ci.chromium.org/ui/p/chromium/builders/try/linux-win-cross-rel/44/overview",
                ],
            ),
            "telemetry_desktop_minidump_unittests": targets.remove(
                reason = "TODO: crbug.com/347165944 - Fix missing dirmd.exe on Linux.",
            ),
            "telemetry_gpu_unittests": targets.remove(
                reason = "TODO: crbug.com/347165944 - Fix missing dirmd.exe on Linux.",
            ),
            "telemetry_perf_unittests": targets.remove(
                reason = "Shadow Win10 Tests x64.",
            ),
            "telemetry_unittests": targets.remove(
                reason = "Shadow Win10 Tests x64.",
            ),
            "webui_resources_tools_python_unittests": targets.remove(
                reason = "Unneeded; only run on non-cross-compiling bots",
            ),
        },
    ),
    cores = 32,
    os = os.LINUX_DEFAULT,
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "misc",
        short_name = "lxw",
    ),
    contact_team_email = "chrome-build-team@google.com",
)
