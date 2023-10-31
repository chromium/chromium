# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.chromiumos builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "siso")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "settings")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.chromiumos",
    pool = try_.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 16,
    compilator_reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 2,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_configs = ["builder"],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
)

consoles.list_view(
    name = "tryserver.chromium.chromiumos",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
)

try_.builder(
    name = "chromeos-amd64-generic-asan-rel",
    mirrors = [
        "ci/chromeos-amd64-generic-asan-rel",
    ],
)

try_.builder(
    name = "chromeos-amd64-generic-cfi-thin-lto-rel",
    mirrors = [
        "ci/chromeos-amd64-generic-cfi-thin-lto-rel",
    ],
)

try_.builder(
    name = "chromeos-amd64-generic-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/chromeos-amd64-generic-dbg",
    ],
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "content/gpu/.+",
            "media/.+",
        ],
    ),
)

try_.builder(
    name = "chromeos-amd64-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a compile only builder for Ash chrome." +
                       " This builder also build Lacros with alternative toolchain.",
    mirrors = ["ci/chromeos-amd64-generic-rel"],
    contact_team_email = "chromeos-sw-engprod@google.com",
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-amd64-generic-rel-gtest",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a Ash chrome builder only run gtest",
    mirrors = [
        "ci/chromeos-amd64-generic-rel",
        "ci/chromeos-amd64-generic-rel-gtest",
    ],
    contact_team_email = "chromeos-sw-engprod@google.com",
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "chromeos-amd64-generic-rel-renamed",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a renamed builder of chromeos-amd64-generic-rel.",
    mirrors = ["ci/chromeos-amd64-generic-rel-renamed"],
    compilator = "chromeos-amd64-generic-rel-compilator",
    contact_team_email = "chromeos-sw-engprod@google.com",
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "chromeos-amd64-generic-rel-compilator",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    cores = "8|16",
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "chromeos-amd64-generic-siso-rel",
    description_html = """\
This builder shadows chromeos-amd64-generic-rel builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating chromeos-amd64-generic-rel from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/chromeos-amd64-generic-rel-renamed"),
    try_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    compilator = "chromeos-amd64-generic-siso-rel-compilator",
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 10,
    ),
)

try_.compilator_builder(
    name = "chromeos-amd64-generic-siso-rel-compilator",
    main_list_view = "try",
    siso_enabled = True,
)

try_.builder(
    name = "chromeos-arm-generic-dbg",
    mirrors = [
        "ci/chromeos-arm-generic-dbg",
    ],
)

try_.builder(
    name = "chromeos-arm-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = ["ci/chromeos-arm-generic-rel"],
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "chromeos-arm64-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = ["ci/chromeos-arm64-generic-rel"],
)

try_.builder(
    name = "lacros-amd64-generic-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-amd64-generic-rel",
    ],
    builderless = not settings.is_main,
    contact_team_email = "chrome-desktop-engprod@google.com",
    main_list_view = "try",
    tryjob = try_.job(),
)

# crbug/1298113: Temporary orchestrator/compilator builders to test
# orchestrator compatability before converting lacros-amd64-generic-rel
try_.orchestrator_builder(
    name = "lacros-amd64-generic-rel-orchestrator",
    description_html = """\
Temporary orchestrator setup for lacros-amd-generic-rel""",
    mirrors = [
        "ci/lacros-amd64-generic-rel",
    ],
    compilator = "lacros-amd64-generic-rel-compilator",
    contact_team_email = "chrome-browser-infra-team@google.com",
    main_list_view = "try",
)

try_.compilator_builder(
    name = "lacros-amd64-generic-rel-compilator",
    description_html = """\
Temporary compilator setup for lacros-amd-generic-rel""",
    contact_team_email = "chrome-browser-infra-team@google.com",
    main_list_view = "try",
)

try_.builder(
    name = "lacros-amd64-generic-rel-non-skylab",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-amd64-generic-rel-non-skylab",
    ],
    contact_team_email = "chrome-desktop-engprod@google.com",
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-amd64-generic-lacros-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/chromeos-amd64-generic-lacros-dbg",
    ],
)

try_.builder(
    name = "lacros-arm-generic-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-arm-generic-rel",
    ],
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "lacros-arm-generic-rel-skylab",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-arm-generic-rel-skylab",
    ],
    contact_team_email = "chrome-desktop-engprod@google.com",
    main_list_view = "try",
)

try_.builder(
    name = "lacros-arm64-generic-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-arm64-generic-rel",
    ],
    main_list_view = "try",
)

try_.builder(
    name = "lacros-arm64-generic-rel-skylab",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-arm64-generic-rel-skylab",
    ],
    contact_team_email = "chrome-desktop-engprod@google.com",
    main_list_view = "try",
)

try_.builder(
    name = "linux-chromeos-compile-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/linux-chromeos-dbg",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    builderless = not settings.is_main,
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(),
)

try_.builder(
    name = "chromeos-jacuzzi-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = [
        "ci/chromeos-jacuzzi-rel",
    ],
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-octopus-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = [
        "ci/chromeos-octopus-rel",
    ],
    main_list_view = "try",
)

try_.builder(
    name = "linux-chromeos-inverse-fieldtrials-fyi-rel",
    mirrors = builder_config.copy_from("try/linux-chromeos-rel"),
)

try_.orchestrator_builder(
    name = "linux-chromeos-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = [
        "ci/linux-chromeos-rel",
    ],
    compilator = "linux-chromeos-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
        "chromium.pre_retry_shards_without_patch_compile": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux-chromeos-rel-compilator",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    cores = 32,
    main_list_view = "try",
    siso_enabled = True,
)

try_.builder(
    name = "linux-lacros-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
    # TODO(crbug.com/1233247) Adds the CI tester when it's available.
    mirrors = [
        "ci/linux-lacros-dbg",
    ],
)

try_.orchestrator_builder(
    name = "linux-lacros-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/linux-lacros-builder-rel",
        "ci/linux-lacros-tester-rel",
    ],
    compilator = "linux-lacros-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux-lacros-rel-compilator",
    branch_selector = branches.selector.CROS_BRANCHES,
    cores = 32,
    main_list_view = "try",
    siso_enabled = True,
)

try_.builder(
    name = "linux-chromeos-dbg",
    # The CI builder that this mirrors is enabled on branches, so this will
    # allow testing changes that would break it before submitting
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/linux-chromeos-dbg",
    ],
)

try_.builder(
    name = "linux-chromeos-annotator-rel",
    mirrors = [
        "ci/linux-chromeos-annotator-rel",
    ],
)

try_.builder(
    name = "linux-cfm-rel",
    mirrors = [
        "ci/linux-cfm-rel",
    ],
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    tryjob = try_.job(
        location_filters = [
            "chromeos/ash/components/chromebox_for_meetings/.+",
            "chromeos/ash/components/dbus/chromebox_for_meetings/.+",
            "chromeos/ash/services/chromebox_for_meetings/.+",
            "chrome/browser/ash/chromebox_for_meetings/.+",
            "chrome/browser/resources/chromeos/chromebox_for_meetings/.+",
            "chrome/browser/ui/webui/ash/chromebox_for_meetings/.+",
            "chrome/test/data/webui/chromeos/chromebox_for_meetings/.+",
        ],
    ),
)
