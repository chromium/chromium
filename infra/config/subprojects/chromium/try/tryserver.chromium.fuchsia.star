# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.fuchsia builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "siso")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/try.star", "try_")
load("//project.star", "settings")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.fuchsia",
    pool = try_.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 8,
    compilator_reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 2,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_configs = ["builder"],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
)

consoles.list_view(
    name = "tryserver.chromium.fuchsia",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
)

try_.builder(
    name = "fuchsia-arm64-cast-receiver-rel",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    mirrors = [
        "ci/fuchsia-arm64-cast-receiver-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/fuchsia-arm64-cast-receiver-rel",
            "release_try_builder",
        ],
    ),
    main_list_view = "try",
    # This is the only bot that builds //chromecast code for Fuchsia on ARM64
    # so trigger it when changes are made.
    tryjob = try_.job(
        location_filters = [
            "chromecast/.+",
        ],
    ),
)

try_.builder(
    name = "fuchsia-arm64-rel",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    mirrors = [
        "ci/fuchsia-arm64-rel",
    ],
    experiments = {
        "enable_weetbix_queries": 100,
        "weetbix.retry_weak_exonerations": 100,
        "weetbix.enable_weetbix_exonerations": 100,
    },
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "reclient",
            "fuchsia",
            "arm64_host",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            # Covers //fuchsia_web and //fuchsia changes, including
            # SDK rolls.
            ".*fuchsia.*",

            # In 04/2022 - 04/2023, there was an independent failure.
            "media/.+",

            # TODO(crbug.com/1377994): When arm64 graphics are supported on
            # emulator, fuchsia-arm64-rel tests should be tested.
            "components/viz/viz.gni",
        ],
    ),
)

try_.builder(
    name = "fuchsia-binary-size",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    executable = "recipe:binary_size_fuchsia_trybot",
    builderless = not settings.is_main,
    cores = 16 if settings.is_main else 8,
    gn_args = gn_args.config(
        configs = [
            "release",
            "official_optimize",
            "reclient",
            "fuchsia",
            "arm64",
            "cast_receiver_size_optimized",
            "use_dummy_lastchange",
        ],
    ),
    properties = {
        "$build/binary_size": {
            "analyze_targets": [
                "//tools/fuchsia/size_tests:fuchsia_sizes",
            ],
            "compile_targets": [
                "fuchsia_sizes",
            ],
        },
    },
    tryjob = try_.job(),
)

# TODO: crbug.com/1502025 - Reduce duplicated configs from the shadow builder.
try_.builder(
    name = "fuchsia-binary-size-siso",
    description_html = """\
This builder shadows fuchsia-binary-size builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating size from Ninja to Siso. b/277863839
""",
    executable = "recipe:binary_size_fuchsia_trybot",
    builderless = False,
    cores = 16,
    contact_team_email = "chrome-build-team@google.com",
    gn_args = "try/fuchsia-binary-size",
    properties = {
        "$build/binary_size": {
            "analyze_targets": [
                "//tools/fuchsia/size_tests:fuchsia_sizes",
            ],
            "compile_targets": [
                "fuchsia_sizes",
            ],
        },
    },
    siso_enabled = True,
    tryjob = try_.job(
        experiment_percentage = 10,
    ),
)

try_.builder(
    name = "fuchsia-compile-x64-dbg",
    mirrors = [
        "ci/fuchsia-x64-dbg",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/fuchsia-x64-dbg",
            "use_dummy_lastchange",
        ],
    ),
    tryjob = try_.job(
        location_filters = [
            "base/fuchsia/.+",
            "fuchsia/.+",
            "media/fuchsia/.+",
        ],
    ),
)

try_.builder(
    name = "fuchsia-deterministic-dbg",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "debug_builder",
            "reclient",
            "fuchsia_smart_display",
        ],
    ),
)

try_.builder(
    name = "fuchsia-fyi-arm64-dbg",
    mirrors = ["ci/fuchsia-fyi-arm64-dbg"],
    gn_args = "ci/fuchsia-fyi-arm64-dbg",
)

try_.builder(
    name = "fuchsia-fyi-x64-asan",
    mirrors = ["ci/fuchsia-fyi-x64-asan"],
    contact_team_email = "chrome-fuchsia-engprod@google.com",
    execution_timeout = 10 * time.hour,
    gn_args = "ci/fuchsia-fyi-x64-asan",
)

try_.builder(
    name = "fuchsia-fyi-x64-dbg",
    mirrors = ["ci/fuchsia-fyi-x64-dbg"],
    gn_args = "ci/fuchsia-fyi-x64-dbg",
)

try_.builder(
    name = "fuchsia-fyi-x64-dbg-persistent-emulator",
    mirrors = ["ci/fuchsia-fyi-x64-dbg-persistent-emulator"],
    contact_team_email = "chrome-fuchsia-engprod@google.com",
    execution_timeout = 10 * time.hour,
    gn_args = "ci/fuchsia-fyi-x64-dbg",
)

try_.orchestrator_builder(
    name = "fuchsia-x64-cast-receiver-rel",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    mirrors = [
        "ci/fuchsia-x64-cast-receiver-rel",
    ],
    compilator = "fuchsia-x64-cast-receiver-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
        "chromium.compilator_can_outlive_parent": 100,
        "chromium.skip_successful_tests": 50,
    },
    gn_args = gn_args.config(
        configs = [
            "ci/fuchsia-x64-cast-receiver-rel",
            "release_try_builder",
            "use_clang_coverage",
            "fuchsia_code_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "fuchsia-x64-cast-receiver-rel-compilator",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    cores = "8|16",
    ssd = True,
    main_list_view = "try",
    siso_enabled = True,
)

try_.builder(
    name = "fuchsia-x64-rel",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    mirrors = [
        "ci/fuchsia-x64-rel",
    ],
    experiments = {
        "enable_weetbix_queries": 100,
        "weetbix.retry_weak_exonerations": 100,
        "weetbix.enable_weetbix_exonerations": 100,
    },
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "reclient",
            "fuchsia",
        ],
    ),
    main_list_view = "try",
)

try_.builder(
    name = "fuchsia-code-coverage",
    mirrors = ["ci/fuchsia-code-coverage"],
    execution_timeout = 20 * time.hour,
    gn_args = "ci/fuchsia-code-coverage",
)
