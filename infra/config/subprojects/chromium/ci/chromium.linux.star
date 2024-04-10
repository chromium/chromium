# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.linux builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "builders", "os", "reclient", "sheriff_rotations", "siso")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

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
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
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
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "tst",
    ),
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-linux-engprod@google.com",
    # TODO(crbug.com/1249968): Roll this out more broadly.
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
    reclient_instance = None,
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
