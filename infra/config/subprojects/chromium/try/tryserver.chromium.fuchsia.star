# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.fuchsia builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os")
load("//lib/consoles.star", "consoles")
load("//lib/try.star", "try_")
load("//project.star", "settings")

try_.defaults.set(
    builder_group = "tryserver.chromium.fuchsia",
    cores = 8,
    orchestrator_cores = 2,
    compilator_cores = 16,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    compilator_goma_jobs = goma.jobs.J150,
    os = os.LINUX_DEFAULT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    name = "tryserver.chromium.fuchsia",
)

# TODO(crbug.com/1294938): Remove this bot after the soft CQ transition.
try_.builder(
    name = "fuchsia-arm64-cast",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chromecast/.+",
        ],
    ),
    mirrors = [
        "ci/fuchsia-arm64-cast",
    ],
)

try_.builder(
    name = "fuchsia-arm64-cast-receiver-rel",
    # TODO(crbug.com/1294938): Make this FUCHSIA_LTS_MILESTONE once the mirrored
    # bot is moved to infra/config/subprojects/chromium/ci/chromium.fuchsia.star.
    branch_selector = branches.MAIN,
    # TODO(crbug.com/1294938): Determine whether this should this have a
    # swarming bot `builder` defined and thus have the following line:
    # builderless = not settings.is_main,
    main_list_view = "try",
    # TODO(crbug.com/1294938): Uncomment the following when removing the
    # `tryjob` attribute from fuchsia-arm64-cast.
    # # This is the only bot that builds //chromecast code for Fuchsia on ARM64
    # # so trigger it when changes are made.
    # tryjob = try_.job(
    #     location_regexp = [
    #         ".+/[+]/chromecast/.+",
    #     ],
    # ),
    mirrors = [
        "ci/fuchsia-arm64-cast-receiver-rel",
    ],
)

try_.builder(
    name = "fuchsia-arm64-rel",
    # TODO(crbug.com/1294938): Make this FUCHSIA_LTS_MILESTONE once the mirrored
    # bot is moved to infra/config/subprojects/chromium/ci/chromium.fuchsia.star.
    branch_selector = branches.MAIN,
    # TODO(crbug.com/1294938): Uncomment this when a swarming bot `builder` with
    # this name is defined:
    # builderless = not settings.is_main,
    main_list_view = "try",
    # TODO(crbug.com/1294938): Uncomment the following when removing the
    # `tryjob` attribute from fuchsia_arm64.
    # tryjob = try_.job(),
    mirrors = [
        "ci/fuchsia-arm64-rel",
    ],
    experiments = {
        "enable_weetbix_queries": 100,
        "weetbix.retry_weak_exonerations": 100,
    },
)

try_.builder(
    name = "fuchsia-binary-size",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    builderless = not settings.is_main,
    cores = 16 if settings.is_main else 8,
    executable = "recipe:binary_size_fuchsia_trybot",
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
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/base/fuchsia/.+",
            ".+/[+]/fuchsia/.+",
            ".+/[+]/media/fuchsia/.+",
        ],
    ),
    mirrors = [
        "ci/fuchsia-x64-dbg",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
)

try_.builder(
    name = "fuchsia-deterministic-dbg",
    executable = "recipe:swarming/deterministic_build",
)

try_.builder(
    name = "fuchsia-fyi-arm64-dbg",
    mirrors = ["ci/fuchsia-fyi-arm64-dbg"],
)

try_.builder(
    name = "fuchsia-fyi-x64-dbg",
    mirrors = ["ci/fuchsia-fyi-x64-dbg"],
)

# TODO(crbug.com/1294938): Remove this bot after the soft CQ transition.
try_.builder(
    name = "fuchsia-x64-cast",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
    mirrors = [
        "ci/fuchsia-x64-cast",
    ],
    experiments = {
        "enable_weetbix_queries": 100,
        "weetbix.retry_weak_exonerations": 100,
    },
)

try_.builder(
    name = "fuchsia-x64-cast-receiver-rel",
    # TODO(crbug.com/1294938): Make this FUCHSIA_LTS_MILESTONE once the mirrored
    # bot is moved to infra/config/subprojects/chromium/ci/chromium.fuchsia.star.
    branch_selector = branches.MAIN,
    # TODO(crbug.com/1294938): Uncomment this when a swarming bot `builder` with
    # this name is defined:
    # builderless = not settings.is_main,
    main_list_view = "try",
    # TODO(crbug.com/1294938): Uncomment the following when removing the
    # `tryjob` attribute from fuchsia-x64-cast.
    # tryjob = try_.job(),
    mirrors = [
        "ci/fuchsia-x64-cast-receiver-rel",
    ],
    experiments = {
        "enable_weetbix_queries": 100,
        "weetbix.retry_weak_exonerations": 100,
    },
)

try_.builder(
    name = "fuchsia-x64-rel",
    # TODO(crbug.com/1294938): Make this FUCHSIA_LTS_MILESTONE once the mirrored
    # bot is moved to infra/config/subprojects/chromium/ci/chromium.fuchsia.star.
    branch_selector = branches.MAIN,
    # TODO(crbug.com/1294938): Uncomment this when a swarming bot `builder` with
    # this name is defined:
    # builderless = not settings.is_main,
    main_list_view = "try",
    # TODO(crbug.com/1294938): Uncomment the following when removing the
    # `tryjob` attribute from fuchsia_x64.
    # tryjob = try_.job(),
    mirrors = [
        "ci/fuchsia-x64-rel",
    ],
    experiments = {
        "enable_weetbix_queries": 100,
        "weetbix.retry_weak_exonerations": 100,
    },
)

# TODO(crbug.com/1294938): Remove this bot after the soft CQ transition.
try_.builder(
    name = "fuchsia_arm64",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
    mirrors = [
        "ci/Fuchsia ARM64",
    ],
    experiments = {
        "enable_weetbix_queries": 100,
        "weetbix.retry_weak_exonerations": 100,
    },
)

# TODO(crbug.com/1294938): Remove this bot after the soft CQ transition.
try_.builder(
    name = "fuchsia_x64",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
    mirrors = [
        "ci/Fuchsia x64",
    ],
    experiments = {
        "enable_weetbix_queries": 100,
        "weetbix.retry_weak_exonerations": 100,
    },
)
