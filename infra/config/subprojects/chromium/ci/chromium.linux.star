# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.linux builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "builders", "os", "reclient", "sheriff_rotations")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.linux",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,
    main_console_view = "main",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    notifies = ["chromium.linux"],
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_remote_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

consoles.console_view(
    name = "chromium.linux",
    branch_selector = branches.selector.LINUX_BRANCHES,
    ordering = {
        None: ["release", "debug"],
        "release": consoles.ordering(short_names = ["bld", "tst", "nsl", "gcc"]),
        "cast": ["x64", "arm64"],
    },
)

targets.builder_defaults.set(
    mixins = ["chromium-tester-service-account"],
)

ci.builder(
    name = "linux-x64-cast-dbg",
    branch_selector = branches.selector.MAIN,
    description_html = "Run Linux and Cast Receiver tests on Linux x64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_receiver",
            "reclient",
            "minimal_symbols",
        ],
    ),
    # TODO(crbug.com/332735845): Garden this once stabilized.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "x64",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "linux-arm64-cast-rel",
    branch_selector = branches.selector.MAIN,
    description_html = "Run Linux and Cast Receiver tests on Linux arm64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "arm64",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_receiver",
            "release_builder",
            "reclient",
            "arm64",
            "minimal_symbols",
        ],
    ),
    # TODO(crbug.com/332735845): Garden this once stabilized.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "Deterministic Linux",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
    ),
    ssd = True,
    free_space = builders.free_space.high,
    # Set tree_closing to false to disable the defaualt tree closer, which
    # filters by step name, and instead enable tree closing for any step
    # failure.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "det",
    ),
    contact_team_email = "chrome-build-team@google.com",
    execution_timeout = 6 * time.hour,
    notifies = ["Deterministic Linux", "close-on-any-step-failure"],
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "Deterministic Linux (dbg)",
    executable = "recipe:swarming/deterministic_build",
    gn_args = {
        "local": "debug_builder",
        "reclient": gn_args.config(
            configs = ["debug_builder", "reclient"],
        ),
    },
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "det",
    ),
    contact_team_email = "chrome-build-team@google.com",
    execution_timeout = 7 * time.hour,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "Leak Detection Linux",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = ["release_builder", "reclient"],
    ),
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        console_view = "chromium.fyi",
        category = "linux",
        short_name = "lk",
    ),
    main_console_view = None,
    notifies = args.ignore_default([]),
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "Linux Builder",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
                # This is necessary due to child builders running the
                # telemetry_perf_unittests suite.
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
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "devtools_do_typecheck",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-linux-engprod@google.com",
)

ci.builder(
    name = "Linux Builder (dbg)",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "debug_builder",
            "reclient",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-linux-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "Linux Builder (Wayland)",
    branch_selector = branches.selector.LINUX_BRANCHES,
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
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "release_builder",
            "reclient",
            "linux_wayland",
            "ozone_headless",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "chrome",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "bld-wl",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-linux-engprod@google.com",
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.thin_tester(
    name = "Linux Tests",
    branch_selector = branches.selector.LINUX_BRANCHES,
    triggered_by = ["ci/Linux Builder"],
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
        build_gs_bucket = "chromium-linux-archive",
    ),
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-linux-engprod@google.com",
    # TODO(crbug.com/40197817): Roll this out more broadly.
    resultdb_bigquery_exports = [
        resultdb.export_text_artifacts(
            bq_table = "chrome-luci-data.chromium.ci_text_artifacts",
            predicate = resultdb.artifact_predicate(
                # Only archive output snippets since some tests can generate
                # very large supplementary files.
                content_type_regexp = "snippet",
            ),
        ),
    ],
)

ci.thin_tester(
    name = "Linux Tests (dbg)(1)",
    branch_selector = branches.selector.LINUX_BRANCHES,
    triggered_by = ["ci/Linux Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_gtests",
            "chromium_linux_dbg_isolated_scripts",
        ],
        mixins = [
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 18,
                ),
            ),
            "browser_tests": targets.mixin(
                # crbug.com/1066161
                # crbug.com/1459645
                # crbug.com/1508286
                swarming = targets.swarming(
                    shards = 32,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 10,
                ),
            ),
            "leveldb_unittests": targets.mixin(
                args = [
                    "--test-launcher-timeout=90000",
                ],
            ),
            "net_unittests": targets.mixin(
                # The suite runs signficantly slower on linux dbg, so increase shards.
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            # TODO(dpranke): Should we be running this step on Linux Tests (dbg)(1)?
            "not_site_per_process_blink_web_tests": targets.remove(
                reason = "removal was present before migration to starlark",
            ),
            "telemetry_perf_unittests": targets.mixin(
                args = [
                    "--xvfb",
                    "--jobs=1",
                ],
            ),
            "webdriver_wpt_tests": targets.mixin(
                args = [
                    "--debug",
                ],
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|tester",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-linux-engprod@google.com",
)

ci.thin_tester(
    name = "Linux Tests (Wayland)",
    branch_selector = branches.selector.LINUX_BRANCHES,
    triggered_by = ["ci/Linux Builder (Wayland)"],
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
        build_gs_bucket = "chromium-linux-archive",
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_gtests",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--no-xvfb",
                    "--use-weston",
                    "--ozone-platform=wayland",
                ],
            ),
            "linux-jammy",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            # https://crbug.com/1084469
            "browser_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/ozone-linux.wayland_browser_tests.filter",
                ],
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1473501
                retry_only_failed_tests = True,
                swarming = targets.swarming(
                    shards = 20,
                ),
            ),
            "content_browsertests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1473501
                retry_only_failed_tests = True,
            ),
            "headless_browsertests": targets.remove(
                reason = "Wayland bot doesn't support headless mode",
            ),
            "interactive_ui_tests": targets.mixin(
                # https://crbug.com/1192997
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/ozone-linux.interactive_ui_tests_wayland.filter",
                ],
            ),
            "ozone_x11_unittests": targets.remove(
                reason = "x11 tests don't make sense for wayland",
            ),
            # https://crbug.com/1184127
            "unit_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/ozone-linux.unit_tests_wayland.filter",
                ],
                # Only retry the individual failed tests instead of rerunning entire
                # shards.
                retry_only_failed_tests = True,
            ),
            "views_unittests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/ozone-linux.wayland_views_unittests.filter",
                ],
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "tst-wl",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-linux-engprod@google.com",
)

ci.builder(
    name = "Network Service Linux",
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
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = ["release_builder", "reclient"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "nsl",
    ),
    contact_team_email = "chrome-linux-engprod@google.com",
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-bfcache-rel",
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
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = ["release_builder_blink", "reclient"],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "bfcache",
        short_name = "bfc",
    ),
    contact_team_email = "chrome-linux-engprod@google.com",
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-extended-tracing-rel",
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
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "extended_tracing",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "trc",
    ),
    contact_team_email = "chrome-linux-engprod@google.com",
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-gcc-rel",
    description_html = "This builder builds only empty_main target to ensure GN config works with is_clang=false.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_no_goma",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "minimal_symbols",
            "no_clang",
        ],
    ),
    # Focal is needed for better C++20 support. See crbug.com/1284275.
    os = os.LINUX_FOCAL,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "gcc",
    ),
    contact_team_email = "build@chromium.org",
)

ci.builder(
    name = "linux-v4l2-codec-rel",
    branch_selector = branches.selector.MAIN,
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
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "v4l2_codec",
            "chrome_with_codecs",
            "release_builder",
            "reclient",
        ],
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    cq_mirrors_console_view = "mirrors",
)
