# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.fuchsia builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "os", "siso")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/html.star", "linkify_builder")
load("//lib/try.star", "try_")
load("//project.star", "settings")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.fuchsia",
    pool = try_.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 8,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 2,
    orchestrator_siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
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
    tryjob = try_.job(
        location_filters = [
            # This is the only bot that builds //chromecast code for Fuchsia on
            # ARM64, so trigger it when changes are made.
            "chromecast/.+",
            # Always trigger this builder when drilling the fuchsia-sdk.
            "build/fuchsia/sdk_override.txt",
        ],
    ),
)

try_.builder(
    name = "fuchsia-binary-size",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    executable = "recipe:binary_size_fuchsia_trybot",
    gn_args = gn_args.config(
        configs = [
            "release",
            "official_optimize",
            "remoteexec",
            "fuchsia",
            "arm64",
            "cast_receiver_size_optimized",
        ],
    ),
    builderless = not settings.is_main,
    cores = 16 if settings.is_main else 8,
    experiments = {
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
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
    # b/325854950 - 1280 concurrent remote jobs might cause slow downloads
    # because this builder doesn't use SSD.
    siso_remote_jobs = 640,
    tryjob = try_.job(),
)

try_.builder(
    name = "fuchsia-x64-cast-receiver-dbg-compile",
    description_html = "A compile only replica of " + linkify_builder("ci", "fuchsia-x64-cast-receiver-dbg", "chromium"),
    mirrors = [
        "ci/fuchsia-x64-cast-receiver-dbg",
    ],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/fuchsia-x64-cast-receiver-dbg",
        ],
    ),
    contact_team_email = "chrome-fuchsia-engprod@google.com",
    tryjob = try_.job(
        location_filters = [
            ".*fuchsia.+",
            cq.location_filter(exclude = True, path_regexp = ".*\\.md"),
        ],
    ),
)

try_.builder(
    name = "fuchsia-deterministic-dbg",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "debug_builder",
            "remoteexec",
            "fuchsia_smart_display",
            "x64",
        ],
    ),
    free_space = builders.free_space.high,
)

try_.builder(
    name = "fuchsia-fyi-arm64-dbg",
    mirrors = ["ci/fuchsia-fyi-arm64-dbg"],
    gn_args = "ci/fuchsia-fyi-arm64-dbg",
)

try_.builder(
    name = "fuchsia-fyi-x64-asan",
    mirrors = ["ci/fuchsia-fyi-x64-asan"],
    gn_args = "ci/fuchsia-fyi-x64-asan",
    contact_team_email = "chrome-fuchsia-engprod@google.com",
    execution_timeout = 10 * time.hour,
)

try_.builder(
    name = "fuchsia-fyi-x64-dbg-persistent-emulator",
    mirrors = ["ci/fuchsia-fyi-x64-dbg-persistent-emulator"],
    gn_args = "ci/fuchsia-fyi-x64-dbg-persistent-emulator",
    contact_team_email = "chrome-fuchsia-engprod@google.com",
    execution_timeout = 10 * time.hour,
)

try_.builder(
    name = "fuchsia-x64-cast-receiver-dbg",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    mirrors = ["ci/fuchsia-x64-cast-receiver-dbg"],
    gn_args = gn_args.config(
        configs = [
            "ci/fuchsia-x64-cast-receiver-dbg",
            "debug_try_builder",
        ],
    ),
    contact_team_email = "chrome-fuchsia-engprod@google.com",
    execution_timeout = 10 * time.hour,
    main_list_view = "try",
    # This is the only bot that builds //chromecast code for Fuchsia on x64
    # so trigger it when changes are made.
    tryjob = try_.job(
        location_filters = [
            "chromecast/.+",
        ],
    ),
)

try_.orchestrator_builder(
    name = "fuchsia-x64-cast-receiver-rel",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    mirrors = [
        "ci/fuchsia-x64-cast-receiver-rel",
    ],
    builder_config_settings = builder_config.try_settings(
        # This is a temporary solution to avoid allowing culprit changes to slip through since
        # retry runs without the patch always fail with connection errors.
        # See https://crbug.com/40278477.
        # TODO(b/40278477): Re-enable the exoneration when the issue above is fixed.
        retry_without_patch = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/fuchsia-x64-cast-receiver-rel",
            "release_try_builder",
            "use_clang_coverage",
            "fuchsia_code_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    compilator = "fuchsia-x64-cast-receiver-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
        "chromium.compilator_can_outlive_parent": 100,
        # crbug.com/940930
        "chromium.enable_cleandead": 100,
        # b/346598710
        "chromium.luci_analysis_v2": 100,
        # crbug.com/355218109
        "chromium.use_per_builder_build_dir_name": 100,
    },
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
)

try_.builder(
    name = "fuchsia-code-coverage",
    mirrors = ["ci/fuchsia-code-coverage"],
    gn_args = "ci/fuchsia-code-coverage",
    execution_timeout = 20 * time.hour,
)
