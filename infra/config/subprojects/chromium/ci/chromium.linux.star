# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.linux builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "builders", "gardener_rotations", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.linux",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    tree_closing = True,
    main_console_view = "main",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    notifies = ["chromium.linux"],
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

consoles.console_view(
    name = "chromium.linux",
    branch_selector = branches.selector.LINUX_BRANCHES,
    ordering = {
        None: ["release", "debug"],
        "release": consoles.ordering(short_names = ["bld", "tst", "nsl", "gcc"]),
        "cast": ["arm64", "x64"],
    },
)

targets.builder_defaults.set(
    mixins = ["chromium-tester-service-account"],
)

ci.builder(
    name = "linux-cast-arm-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    description_html = "Run Linux and Cast Receiver build on Linux ARM",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "arm",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium_clang",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_linux",
            "cast_release",
            "remoteexec",
            "arm",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_cast_receiver",
        ],
    ),
    # TODO(vigeni): Remove as configuration has been stablized.
    gardener_rotations = args.ignore_default(None),
    # TODO(vigeni): Set to True configuration has been stablized.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "arm32rel",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "linux-cast-arm64-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    description_html = "Run Linux and Cast Receiver build on Linux ARM64",
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
            "cast_linux",
            "cast_release",
            "remoteexec",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_cast_receiver",
        ],
    ),
    # TODO(vigeni): Remove as configuration has been stablized.
    gardener_rotations = args.ignore_default(None),
    # TODO(vigeni): Set to True configuration has been stablized.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "arm64rel",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "linux-cast-x64-dbg",
    branch_selector = branches.selector.LINUX_BRANCHES,
    description_html = "Run Linux and Cast Receiver tests on Linux x64 Debug",
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
        build_gs_bucket = "chromium-linux-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cast_linux",
            "cast_debug",
            "remoteexec",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_cast_receiver",
            "chromium_linux_cast_receiver_gtests",
        ],
    ),
    # TODO(vigeni): Remove as configuration has been stablized.
    gardener_rotations = args.ignore_default(None),
    # TODO(vigeni): Set to True configuration has been stablized.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "x64dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "cast-eng@google.com",
)

ci.builder(
    name = "linux-cast-x64-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    description_html = "Run Linux and Cast Receiver tests on Linux x64 Release",
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
            "cast_linux",
            "cast_release",
            "remoteexec",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_cast_receiver",
            "chromium_linux_cast_receiver_gtests",
        ],
    ),
    # TODO(vigeni): Remove as configuration has been stablized.
    gardener_rotations = args.ignore_default(None),
    # TODO(vigeni): Set to True configuration has been stablized.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "cast",
        short_name = "x64rel",
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
            "remoteexec",
            "minimal_symbols",
            "linux",
            "x64",
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
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

ci.builder(
    name = "Deterministic Linux (dbg)",
    executable = "recipe:swarming/deterministic_build",
    gn_args = {
        "local": gn_args.config(
            configs = ["debug_builder", "linux", "x64"],
        ),
        "reclient": gn_args.config(
            configs = ["debug_builder", "remoteexec", "linux", "x64"],
        ),
    },
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "det",
    ),
    contact_team_email = "chrome-build-team@google.com",
    execution_timeout = 7 * time.hour,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
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
        configs = [
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        console_view = "chromium.fyi",
        category = "linux",
        short_name = "lk",
    ),
    main_console_view = None,
    notifies = args.ignore_default([]),
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
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
            "remoteexec",
            "devtools_do_typecheck",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_scripts",
        ],
        additional_compile_targets = [
            "all",
        ],
        mixins = [
            "isolate_profile_data",
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
            apply_configs = [
                # This is necessary due to child builders running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
            ],
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
            "remoteexec",
            "linux",
            "x64",
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
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            "remoteexec",
            "linux_wayland",
            "ozone_headless",
            "x64",
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
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
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
    targets = targets.bundle(
        targets = [
            "chromium_linux_gtests",
            "chromium_linux_rel_isolated_scripts_once",
        ],
        mixins = [
            "isolate_profile_data",
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_web_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m.profraw",
                ],
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m.profraw",
                ],
                swarming = targets.swarming(
                    shards = 10,
                ),
            ),
            "browser_tests": targets.mixin(
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
            "not_site_per_process_blink_web_tests": targets.mixin(
                args = [
                    "--additional-env-var=LLVM_PROFILE_FILE=${ISOLATED_OUTDIR}/profraw/default-%2m.profraw",
                ],
            ),
            "telemetry_perf_unittests": targets.mixin(
                args = [
                    "--xvfb",
                    "--jobs=1",
                ],
            ),
            "unit_tests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1473501
                retry_only_failed_tests = True,
            ),
            "webdriver_wpt_tests": targets.remove(
                reason = "https://crbug.com/929689, https://crbug.com/936557",
            ),
        },
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
                    shards = 15,
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
        configs = [
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "nsl",
    ),
    contact_team_email = "chrome-linux-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
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
        configs = [
            "release_builder_blink",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "bfcache",
        short_name = "bfc",
    ),
    contact_team_email = "chrome-linux-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
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
            "remoteexec",
            "extended_tracing",
            "linux",
            "x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "trc",
    ),
    contact_team_email = "chrome-linux-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
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
            "minimal_symbols",
            "no_clang",
            "linux",
            "x64",
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
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    cq_mirrors_console_view = "mirrors",
)
