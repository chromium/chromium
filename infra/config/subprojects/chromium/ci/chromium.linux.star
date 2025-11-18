# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.linux builder group."""

load("@chromium-luci//args.star", "args")
load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "builders", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/gardener_rotations.star", "gardener_rotations")
load("//lib/siso.star", "siso")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.linux",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    pool = ci_constants.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    tree_closing = True,
    tree_closing_notifiers = ci_constants.DEFAULT_TREE_CLOSING_NOTIFIERS,
    main_console_view = "main",
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    notifies = ["chromium.linux"],
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

consoles.console_view(
    name = "chromium.linux",
    branch_selector = branches.selector.LINUX_BRANCHES,
    ordering = {
        None: ["release", "debug"],
        "release": consoles.ordering(short_names = ["bld", "tst", "nsl", "gcc"]),
        "cast": ["arm", "arm64", "x64"],
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
    tree_closing = True,
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
    tree_closing = True,
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
    tree_closing = True,
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
    tree_closing = True,
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
    # Don't use the default tree closer, which filters by step name, and instead
    # enable tree closing for any step failure.
    tree_closing_notifiers = args.ignore_default(["close-on-any-step-failure"]),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "det",
    ),
    contact_team_email = "chrome-build-team@google.com",
    execution_timeout = 6 * time.hour,
    notifies = ["Deterministic Linux"],
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
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "leak_detection_isolated_scripts",
        ],
        mixins = [
            "linux-jammy",
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
    siso_remote_linking = True,
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
                "checkout_mutter",
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
    parent = "ci/Linux Builder",
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
    ),
    targets = targets.bundle(
        targets = [
            "chromium_linux_gtests",
            "chromium_linux_rel_isolated_scripts_once",
            "gtests_once",
        ],
        mixins = [
            "isolate_profile_data",
            "linux-jammy",
            "retry_only_failed_tests",
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
            ),
            "browser_tests": targets.mixin(
                swarming = targets.swarming(
                    shards = 20,
                ),
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
            "webdriver_wpt_tests": targets.mixin(
                ci_only = True,
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
    parent = "ci/Linux Builder (dbg)",
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
            "browser_tests": targets.mixin(
                # crbug.com/1066161
                # crbug.com/1459645
                # crbug.com/1508286
                # crbug.com/404871436
                swarming = targets.swarming(
                    shards = 60,
                ),
            ),
            "content_browsertests": targets.mixin(
                # crbug.com/1508286
                # crbug.com/404871436
                swarming = targets.swarming(
                    shards = 12,
                ),
            ),
            "content_unittests": targets.mixin(
                swarming = targets.swarming(
                    shards = 4,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/ozone-linux.interactive_ui_tests.filter",
                ],
                swarming = targets.swarming(
                    shards = 20,
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
            "unit_tests": targets.mixin(
                # The suite runs signficantly slower on linux dbg, so increase shards.
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "webdriver_wpt_tests": targets.mixin(
                args = [
                    "--debug",
                ],
                swarming = targets.swarming(
                    shards = 4,
                ),
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
    name = "linux-wayland-weston-rel-tests",
    branch_selector = branches.selector.LINUX_BRANCHES,
    description_html =
        "Runs Wayland tests on Weston. See the {} for details.".format(
            linkify(
                "https://chromium.googlesource.com/chromium/src/+/main/docs/ozone_overview.md#wayland",
                "ozone wayland doc",
            ),
        ),
    parent = "ci/Linux Builder (Wayland)",
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
                    "--test-launcher-filter-file=../../testing/buildbot/filters/ozone-linux.browser_tests_weston.filter",
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
                    "--test-launcher-filter-file=../../testing/buildbot/filters/ozone-linux.interactive_ui_tests_weston.filter",
                    # TODO(crbug.com/334413759) Until bubble subsurfaces can be
                    # properly tested using weston, disable the feature when
                    # running tests there.
                    "--disable-accelerated-subwindows-for-testing",
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

ci.thin_tester(
    name = "linux-wayland-mutter-rel-tests",
    branch_selector = branches.selector.LINUX_BRANCHES,
    description_html =
        "Runs Wayland tests on Mutter. See the {} for details.".format(
            linkify(
                "https://chromium.googlesource.com/chromium/src/+/main/docs/ozone_overview.md#wayland",
                "ozone wayland doc",
            ),
        ),
    parent = "ci/Linux Builder (Wayland)",
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
    ),
    targets = targets.bundle(
        targets = [
            "chromium_gtests_for_linux_wayland_mutter",
        ],
        mixins = [
            targets.mixin(
                args = [
                    "--no-xvfb",
                    "--use-mutter",
                    "--ozone-platform=wayland",
                ],
            ),
            "linux-noble",
            "isolate_profile_data",
        ],
        per_test_modifications = {
            # https://crbug.com/1084469
            "browser_tests": targets.mixin(
                args = [
                    # crbug.com/414750476 PDF extension tests fail with the
                    # default 1920x1200 resolution used for mutter.
                    "--mutter-display=1280x800",
                    "--test-launcher-filter-file=../../testing/buildbot/filters/ozone-linux.browser_tests_mutter.filter",
                ],
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1473501
                retry_only_failed_tests = True,
                swarming = targets.swarming(
                    expiration_sec = 18000,
                    hard_timeout_sec = 14400,
                    shards = 46,
                ),
            ),
            "content_browsertests": targets.mixin(
                # Only retry the individual failed tests instead of rerunning
                # entire shards.
                # crbug.com/1473501
                retry_only_failed_tests = True,
                swarming = targets.swarming(
                    expiration_sec = 18000,
                    hard_timeout_sec = 14400,
                    shards = 8,
                ),
            ),
            "interactive_ui_tests": targets.mixin(
                # https://crbug.com/1192997
                args = [
                    "--test-launcher-filter-file=../../testing/buildbot/filters/ozone-linux.interactive_ui_tests_mutter.filter",
                ],
                swarming = targets.swarming(
                    expiration_sec = 18000,
                    hard_timeout_sec = 14400,
                    shards = 12,
                ),
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "tst-mt",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-linux-engprod@google.com",
)

# For documentation, see //services/network/README.md.
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
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "network_service_extra_gtests",
        ],
        mixins = [
            "linux-jammy",
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
    name = "linux-oi-rel",
    description_html = "This builder runs key test suites with OriginKeyedProcessesByDefault (OriginIsolation) enabled, to provide test coverage with the feature enabled.",
    parent = "ci/Linux Builder",
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
    ),
    targets = targets.bundle(
        name = "linux_oi_tests",
        targets = [
            "browser_tests",
            "unit_tests",
            "content_browsertests",
            "content_unittests",
            "blink_web_tests",
            "blink_wpt_tests",
            "chrome_wpt_tests",
        ],
        mixins = [
            "linux-jammy",
        ],
        per_test_modifications = {
            "browser_tests": targets.mixin(
                args = [
                    "--enable-features=OriginKeyedProcessesByDefault",
                ],
                swarming = targets.swarming(
                    shards = 33,
                ),
            ),
            "unit_tests": targets.mixin(
                args = [
                    "--enable-features=OriginKeyedProcessesByDefault",
                ],
                # Default shards = 1 should be ok here.
            ),
            "content_browsertests": targets.mixin(
                args = [
                    "--enable-features=OriginKeyedProcessesByDefault",
                ],
                swarming = targets.swarming(
                    shards = 8,
                ),
            ),
            "content_unittests": targets.mixin(
                args = [
                    "--enable-features=OriginKeyedProcessesByDefault",
                ],
                # Default shards = 1 should be ok here.
            ),
            "blink_web_tests": targets.mixin(
                args = [
                    "--additional-driver-flag=--enable-features=OriginKeyedProcessesByDefault",
                ],
                swarming = targets.swarming(
                    shards = 9,
                ),
            ),
            "blink_wpt_tests": targets.mixin(
                args = [
                    "--additional-driver-flag=--enable-features=OriginKeyedProcessesByDefault",
                ],
                swarming = targets.swarming(
                    shards = 2,
                ),
            ),
            "chrome_wpt_tests": targets.mixin(
                args = [
                    "--additional-driver-flag=--enable-features=OriginKeyedProcessesByDefault",
                ],
                # Default shards = 1 should be ok here.
            ),
        },
    ),
    console_view_entry = consoles.console_view_entry(
        category = "OriginIsolation",
        short_name = "oi",
    ),
    contact_team_email = "chrome-security-architecture@google.com",
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
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder_blink",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "bfcache_linux_gtests",
            "chromium_webkit_isolated_scripts",
        ],
        mixins = [
            "linux-jammy",
        ],
        per_test_modifications = {
            "blink_wpt_tests": targets.mixin(
                args = [
                    # TODO(crbug.com/40200069): Re-enable the test.
                    "--ignore-tests=external/wpt/html/browsers/browsing-the-web/back-forward-cache/events.html",
                ],
            ),
        },
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
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
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
    targets = targets.bundle(
        additional_compile_targets = [
            "empty_main",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "gcc",
    ),
    contact_team_email = "build@chromium.org",
)

ci.builder(
    name = "linux-no-modules-compile-rel",
    branch_selector = branches.selector.MAIN,
    description_html = "Experimental compile with use_clang_modules=false.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/Linux Builder",
            "no_clang_modules",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "all",
        ],
    ),
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        console_view = "chromium.fyi",
        category = "linux",
        short_name = "mod",
    ),
    main_console_view = None,
    contact_team_email = "chrome-build-team@google.com",
    execution_timeout = 6 * time.hour,
    notifies = args.ignore_default([]),
    siso_keep_going = True,
    siso_remote_linking = True,
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
    targets = targets.bundle(
        additional_compile_targets = [
            "image_processor_perf_test",
            "video_decode_accelerator_tests",
            "video_decode_accelerator_perf_tests",
            "video_encode_accelerator_tests",
            "video_encode_accelerator_perf_tests",
            "v4l2_stateless_decoder",
            "v4l2_unittest",
        ],
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
    ),
    cq_mirrors_console_view = "mirrors",
)
