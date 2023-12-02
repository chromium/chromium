# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.chromiumos builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_url.star", "linkify_builder")
load("//lib/builders.star", "os", "reclient", "siso")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
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
    gn_args = "ci/chromeos-amd64-generic-asan-rel",
)

try_.builder(
    name = "chromeos-amd64-generic-cfi-thin-lto-rel",
    mirrors = [
        "ci/chromeos-amd64-generic-cfi-thin-lto-rel",
    ],
    # TODO(crbug.com/913750): Enable DCHECKS on the two amd64-generic bots
    # when the PFQ has it enabled.
    gn_args = "ci/chromeos-amd64-generic-cfi-thin-lto-rel",
)

try_.builder(
    name = "chromeos-amd64-generic-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/chromeos-amd64-generic-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/chromeos-amd64-generic-dbg",
            "use_dummy_lastchange",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "ci/chromeos-amd64-generic-rel",
            "dcheck_always_on",
            "use_dummy_lastchange",
        ],
    ),
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "chromeos-amd64-generic-rel-gtest",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is an Ash chrome builder which only runs gtest." +
                       " This builder is the default CQ builder for" +
                       " non-ChromeOS engineers only. See the builder" +
                       " description for " +
                       linkify_builder("try", "chromeos-amd64-generic-rel-gtest-and-tast", "chromium") +
                       " for more information",
    mirrors = [
        "ci/chromeos-amd64-generic-rel",
        "ci/chromeos-amd64-generic-rel-gtest",
    ],
    compilator = "chromeos-amd64-generic-rel-gtest-compilator",
    contact_team_email = "chromeos-sw-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "ci/chromeos-amd64-generic-rel",
            "dcheck_always_on",
            "use_dummy_lastchange",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        equivalent_builder = "try/chromeos-amd64-generic-rel-gtest-and-tast",
        equivalent_builder_percentage = 100,
        equivalent_builder_whitelist = "google/chromeos-pa@google.com",
        # Use dummypath to make sure it's not auto triggered.
        location_filters = ["dummypath/.+"],
    ),
)

try_.orchestrator_builder(
    name = "chromeos-amd64-generic-rel-gtest-and-tast",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is an Ash chrome builder which runs gtest" +
                       " and Tast tests. This builder is the default CQ" +
                       " builder for ChromeOS engineers only." +
                       " For a CL, infra would check the CL’s owner to see" +
                       " if the owner is a ChromeOS org engineer or not." +
                       " If the owner is a ChromeOS org engineer, the" +
                       " default CQ would include this builder which runs" +
                       " both Tast tests and gtests. Otherwise, the default" +
                       " CQ would include `chromeos-amd64-generic-rel-gtest`" +
                       " which only runs gtests.",
    mirrors = [
        "ci/chromeos-amd64-generic-rel",
        "ci/chromeos-amd64-generic-rel-gtest",
        "ci/chromeos-amd64-generic-rel-tast",
    ],
    compilator = "chromeos-amd64-generic-rel-gtest-and-tast-compilator",
    contact_team_email = "chromeos-sw-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "ci/chromeos-amd64-generic-rel",
            "dcheck_always_on",
            "use_dummy_lastchange",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        omit_from_luci_cv = True,
    ),
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
        "chromium.skip_successful_tests": 50,
    },
    gn_args = gn_args.config(
        configs = [
            "ci/chromeos-amd64-generic-rel-renamed",
            "dcheck_always_on",
            "use_dummy_lastchange",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(),
)

CHROMEOS_SHARED_CACHE = "shared_chromeos_amd64_generic_rel_cache"

try_.compilator_builder(
    name = "chromeos-amd64-generic-rel-compilator",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    cores = "8|16",
    caches = [
        swarming.cache(
            name = CHROMEOS_SHARED_CACHE,
            path = "builder",
            wait_for_warm_cache = 4 * time.minute,
        ),
    ],
    main_list_view = "try",
)

try_.compilator_builder(
    name = "chromeos-amd64-generic-rel-gtest-compilator",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = ".",
    cores = "16",
    caches = [
        swarming.cache(
            name = CHROMEOS_SHARED_CACHE,
            path = "builder",
            wait_for_warm_cache = 4 * time.minute,
        ),
    ],
    contact_team_email = "chromeos-sw-engprod@google.com",
    main_list_view = "try",
)

try_.compilator_builder(
    name = "chromeos-amd64-generic-rel-gtest-and-tast-compilator",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = ".",
    cores = "16",
    caches = [
        swarming.cache(
            name = CHROMEOS_SHARED_CACHE,
            path = "builder",
            wait_for_warm_cache = 4 * time.minute,
        ),
    ],
    contact_team_email = "chromeos-sw-engprod@google.com",
    main_list_view = "try",
)

# TODO: crbug.com/1502025 - Reduce duplicated configs from the shadow builder.
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
    gn_args = "ci/chromeos-arm-generic-dbg",
)

try_.builder(
    name = "chromeos-arm-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = ["ci/chromeos-arm-generic-rel"],
    builderless = not settings.is_main,
    experiments = {
        "chromium.skip_successful_tests": 50,
    },
    gn_args = gn_args.config(
        configs = [
            "ci/chromeos-arm-generic-rel",
            "dcheck_always_on",
            "use_dummy_lastchange",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "chromeos-arm64-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = ["ci/chromeos-arm64-generic-rel"],
    gn_args = "ci/chromeos-arm64-generic-rel",
)

try_.orchestrator_builder(
    name = "lacros-amd64-generic-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    description_html = """\
Lacros builder that runs Tast tests and gtests on ChromeOS devices via Skylab""",
    mirrors = [
        "ci/lacros-amd64-generic-rel",
    ],
    compilator = "lacros-amd64-generic-rel-compilator",
    contact_team_email = "chrome-desktop-engprod@google.com",
    experiments = {
        "chromium.skip_successful_tests": 50,
    },
    gn_args = gn_args.config(
        configs = [
            "ci/lacros-amd64-generic-rel",
            "dcheck_always_on",
            "use_dummy_lastchange",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "lacros-amd64-generic-rel-compilator",
    branch_selector = branches.selector.CROS_BRANCHES,
    builderless = not settings.is_main,
    cores = 8,
    contact_team_email = "chrome-desktop-engprod@google.com",
    main_list_view = "try",
)

try_.builder(
    name = "lacros-amd64-generic-rel-non-skylab",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-amd64-generic-rel-non-skylab",
    ],
    contact_team_email = "chrome-desktop-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "ci/lacros-amd64-generic-rel-non-skylab",
            "dcheck_always_on",
            "use_dummy_lastchange",
        ],
    ),
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-amd64-generic-lacros-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/chromeos-amd64-generic-lacros-dbg",
    ],
    gn_args = "ci/chromeos-amd64-generic-lacros-dbg",
)

try_.builder(
    name = "lacros-arm-generic-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-arm-generic-rel",
    ],
    builderless = not settings.is_main,
    experiments = {
        "chromium.skip_successful_tests": 50,
    },
    gn_args = gn_args.config(
        configs = [
            "ci/lacros-arm-generic-rel",
            "dcheck_always_on",
            "use_dummy_lastchange",
        ],
    ),
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
    gn_args = "ci/lacros-arm-generic-rel-skylab",
    main_list_view = "try",
)

try_.builder(
    name = "lacros-arm64-generic-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-arm64-generic-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/lacros-arm64-generic-rel",
            "dcheck_always_on",
        ],
    ),
    main_list_view = "try",
)

try_.builder(
    name = "lacros-arm64-generic-rel-skylab",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-arm64-generic-rel-skylab",
    ],
    contact_team_email = "chrome-desktop-engprod@google.com",
    gn_args = "ci/lacros-arm64-generic-rel-skylab",
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
    experiments = {
        "chromium.skip_successful_tests": 50,
    },
    gn_args = gn_args.config(
        configs = [
            "ci/linux-chromeos-dbg",
            "no_symbols",
            "use_dummy_lastchange",
        ],
    ),
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(),
)

# TODO: crbug.com/1502025 - Reduce duplicated configs from the shadow builder.
try_.builder(
    name = "linux-chromeos-compile-siso-dbg",
    description_html = """\
This builder shadows linux-chromeos-compile-dbg builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating linux-chromeos-compile-dbg from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/linux-chromeos-compile-dbg"),
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    contact_team_email = "chrome-build-team@google.com",
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    siso_enabled = True,
    tryjob = try_.job(
        experiment_percentage = 10,
    ),
)

try_.builder(
    name = "chromeos-jacuzzi-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = [
        "ci/chromeos-jacuzzi-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/chromeos-jacuzzi-rel",
            "dcheck_always_on",
        ],
    ),
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-jacuzzi-rel-skylab",
    branch_selector = branches.selector.MAIN,
    description_html = "This is a builder that runs HW test on Skylab." +
                       " This builder also build Lacros with alternative toolchain.",
    mirrors = [
        "ci/chromeos-jacuzzi-rel-skylab-fyi",
    ],
    contact_team_email = "chromeos-velocity@google.com",
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-octopus-rel-skylab",
    branch_selector = branches.selector.MAIN,
    description_html = "This builder builds public image and runs tests on octopus DUTs in the lab.<br/>" +
                       "This is experimental.",
    mirrors = [
        "ci/chromeos-octopus-rel-skylab-fyi",
    ],
    contact_team_email = "chromeos-velocity@google.com",
    gn_args = "ci/chromeos-octopus-rel-skylab-fyi",
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-octopus-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = [
        "ci/chromeos-octopus-rel",
    ],
    gn_args = "ci/chromeos-octopus-rel",
    main_list_view = "try",
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
        "chromium.skip_successful_tests": 50,
    },
    gn_args = gn_args.config(
        configs = [
            "ci/linux-chromeos-rel",
            "release_try_builder",
            "no_symbols",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
            "enable_dangling_raw_ptr_feature_flag",
            "enable_backup_ref_ptr_feature_flag",
        ],
    ),
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
    gn_args = "ci/linux-lacros-dbg",
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
        "chromium.skip_successful_tests": 50,
    },
    gn_args = gn_args.config(
        configs = [
            "ci/linux-lacros-builder-rel",
            "release_try_builder",
            "clang",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
            "use_dummy_lastchange",
        ],
    ),
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
    gn_args = gn_args.config(
        configs = [
            "ci/linux-chromeos-dbg",
            "use_dummy_lastchange",
        ],
    ),
)

try_.builder(
    name = "linux-chromeos-annotator-rel",
    mirrors = [
        "ci/linux-chromeos-annotator-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-chromeos-annotator-rel",
            "try_builder",
            "no_symbols",
            "also_build_lacros_chrome",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
            "enable_dangling_raw_ptr_feature_flag",
            "enable_backup_ref_ptr_feature_flag",
        ],
    ),
)

try_.builder(
    name = "linux-cfm-rel",
    mirrors = [
        "ci/linux-cfm-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-cfm-rel",
            "release_try_builder",
        ],
    ),
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
