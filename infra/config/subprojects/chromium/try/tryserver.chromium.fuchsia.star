# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.fuchsia builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "reclient")
load("//lib/consoles.star", "consoles")
load("//lib/try.star", "try_")
load("//project.star", "settings")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.fuchsia",
    pool = try_.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 8,
    compilator_goma_jobs = goma.jobs.J150,
    compilator_reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
    orchestrator_cores = 2,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
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
    goma_backend = None,
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
    name = "fuchsia-arm64-chrome-rel",
    mirrors = [
        "ci/fuchsia-arm64-chrome-rel",
    ],
)

try_.orchestrator_builder(
    name = "fuchsia-arm64-rel",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    mirrors = [
        "ci/fuchsia-arm64-rel",
    ],
    compilator = "fuchsia-arm64-rel-compilator",
    experiments = {
        "enable_weetbix_queries": 100,
        "weetbix.retry_weak_exonerations": 100,
        "weetbix.enable_weetbix_exonerations": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "fuchsia-arm64-rel-compilator",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    # TODO(crbug.com/1298110): Set to True once compilator bots are moved
    ssd = None,
    main_list_view = "try",
)

try_.builder(
    name = "fuchsia-binary-size",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    executable = "recipe:binary_size_fuchsia_trybot",
    builderless = not settings.is_main,
    cores = 16 if settings.is_main else 8,
    goma_backend = None,
    goma_jobs = goma.jobs.J150,
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

try_.builder(
    name = "fuchsia-compile-x64-dbg",
    mirrors = [
        "ci/fuchsia-x64-dbg",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    goma_backend = None,
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
    goma_backend = None,
)

try_.builder(
    name = "fuchsia-fyi-arm64-dbg",
    mirrors = ["ci/fuchsia-fyi-arm64-dbg"],
    goma_backend = None,
)

try_.builder(
    name = "fuchsia-fyi-x64-dbg",
    mirrors = ["ci/fuchsia-fyi-x64-dbg"],
    goma_backend = None,
)

try_.orchestrator_builder(
    name = "fuchsia-x64-cast-receiver-rel",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    mirrors = [
        "ci/fuchsia-x64-cast-receiver-rel",
    ],
    compilator = "fuchsia-x64-cast-receiver-rel-compilator",
    experiments = {
        "enable_weetbix_queries": 100,
        "weetbix.retry_weak_exonerations": 100,
        "weetbix.enable_weetbix_exonerations": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "fuchsia-x64-cast-receiver-rel-compilator",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    cores = "8|16",
    ssd = True,
    goma_backend = None,
    main_list_view = "try",
)

try_.builder(
    name = "fuchsia-x64-chrome-rel",
    mirrors = [
        "ci/fuchsia-x64-chrome-rel",
    ],
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
    main_list_view = "try",
)

try_.builder(
    name = "fuchsia-code-coverage",
    mirrors = ["ci/fuchsia-code-coverage"],
    execution_timeout = 20 * time.hour,
)
