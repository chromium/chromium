# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.chromiumos builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "settings")

try_.defaults.set(
    builder_group = "tryserver.chromium.chromiumos",
    cores = 8,
    orchestrator_cores = 2,
    compilator_cores = 16,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_DEFAULT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
)

consoles.list_view(
    name = "tryserver.chromium.chromiumos",
    branch_selector = branches.CROS_LTS_MILESTONE,
)

try_.builder(
    name = "chromeos-amd64-generic-cfi-thin-lto-rel",
    mirrors = [
        "ci/chromeos-amd64-generic-cfi-thin-lto-rel",
    ],
)

try_.builder(
    name = "chromeos-amd64-generic-dbg",
    branch_selector = branches.STANDARD_MILESTONE,
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

try_.orchestrator_builder(
    name = "chromeos-amd64-generic-rel",
    compilator = "chromeos-amd64-generic-rel-compilator",
    branch_selector = branches.CROS_LTS_MILESTONE,
    mirrors = ["ci/chromeos-amd64-generic-rel"],
    main_list_view = "try",
    tryjob = try_.job(),
    experiments = {
        "remove_src_checkout_experiment": 100,
    },
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "chromeos-amd64-generic-rel-compilator",
    branch_selector = branches.CROS_LTS_MILESTONE,
    main_list_view = "try",
    cores = 16,
)

try_.builder(
    name = "chromeos-arm-generic-dbg",
    mirrors = [
        "ci/chromeos-arm-generic-dbg",
    ],
)

try_.builder(
    name = "chromeos-arm-generic-rel",
    branch_selector = branches.CROS_LTS_MILESTONE,
    mirrors = ["ci/chromeos-arm-generic-rel"],
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "chromeos-arm64-generic-rel",
    branch_selector = branches.CROS_LTS_MILESTONE,
    mirrors = ["ci/chromeos-arm64-generic-rel"],
)

try_.builder(
    name = "lacros-amd64-generic-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/lacros-amd64-generic-rel",
    ],
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.orchestrator_builder(
    name = "lacros-amd64-generic-rel-orchestrator",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/lacros-amd64-generic-rel",
    ],
    compilator = "lacros-amd64-generic-rel-compilator",
    main_list_view = "try",
    experiments = {
        "remove_src_checkout_experiment": 100,
    },
    use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "lacros-amd64-generic-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    # TODO (crbug.com/1287228): Set correct values once bots are set up
    ssd = None,
    cores = None,
)

try_.builder(
    name = "lacros-amd64-generic-rel-skylab-fyi",
    branch_selector = branches.STANDARD_MILESTONE,
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mb_no_luci_auth"],
            target_bits = 64,
            target_cros_boards = "eve",
            cros_boards_with_qemu_images = "amd64-generic",
            target_platform = "chromeos",
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "checkout_lacros_sdk",
            ],
        ),
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "gs://lacros-amd64-generic-rel-skylab-try",
        ),
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
        ),
        build_gs_bucket = "chromium-fyi-archive",
    ),
    builderless = not settings.is_main,
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-amd64-generic-lacros-dbg",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/chromeos-amd64-generic-lacros-dbg",
    ],
)

try_.builder(
    name = "lacros-arm-generic-rel",
    mirrors = [
        "ci/lacros-arm-generic-rel",
    ],
    branch_selector = branches.STANDARD_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "lacros-arm64-generic-rel",
    mirrors = [
        "ci/lacros-arm64-generic-rel",
    ],
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
)

try_.builder(
    name = "linux-chromeos-compile-dbg",
    mirrors = [
        "ci/linux-chromeos-dbg",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    branch_selector = branches.STANDARD_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "linux-lacros-tester-rel-reviver",
    mirrors = [
        "ci/linux-lacros-tester-rel-reviver",
    ],
    builderless = True,
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-kevin-compile-rel",
    mirrors = [
        "ci/chromeos-kevin-rel",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
)

try_.builder(
    name = "chromeos-jacuzzi-rel",
    branch_selector = branches.CROS_LTS_MILESTONE,
    mirrors = [
        "ci/chromeos-jacuzzi-rel",
    ],
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-kevin-rel",
    branch_selector = branches.CROS_LTS_MILESTONE,
    mirrors = [
        "ci/chromeos-kevin-rel",
    ],
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "build/chromeos/.+",
            "build/config/chromeos/.*",
            "chromeos/CHROMEOS_LKGM",
        ],
    ),
)

try_.builder(
    name = "chromeos-octopus-rel",
    branch_selector = branches.CROS_LTS_MILESTONE,
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
    mirrors = [
        "ci/linux-chromeos-rel",
    ],
    compilator = "linux-chromeos-rel-compilator",
    branch_selector = branches.CROS_LTS_MILESTONE,
    main_list_view = "try",
    use_clang_coverage = True,
    coverage_test_types = ["unit", "overall"],
    tryjob = try_.job(),
    experiments = {
        "remove_src_checkout_experiment": 100,
    },
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux-chromeos-rel-compilator",
    branch_selector = branches.CROS_LTS_MILESTONE,
    main_list_view = "try",
    goma_jobs = goma.jobs.J300,
    cores = 32,
)

try_.builder(
    name = "linux-lacros-dbg",
    # TODO(crbug.com/1233247) Adds the CI tester when it's available.
    mirrors = [
        "ci/linux-lacros-dbg",
    ],
)

try_.orchestrator_builder(
    name = "linux-lacros-rel",
    mirrors = [
        "ci/linux-lacros-builder-rel",
        "ci/linux-lacros-tester-rel",
    ],
    branch_selector = branches.STANDARD_MILESTONE,
    compilator = "linux-lacros-rel-compilator",
    check_for_flakiness = True,
    main_list_view = "try",
    tryjob = try_.job(),
    experiments = {
        "remove_src_checkout_experiment": 100,
    },
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux-lacros-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    goma_jobs = goma.jobs.J300,
)

try_.builder(
    name = "linux-chromeos-dbg",
    mirrors = [
        "ci/linux-chromeos-dbg",
    ],
    # The CI builder that this mirrors is enabled on branches, so this will
    # allow testing changes that would break it before submitting
    branch_selector = branches.STANDARD_MILESTONE,
)

try_.builder(
    name = "linux-chromeos-annotator-rel",
)

try_.builder(
    name = "linux-cfm-rel",
    mirrors = [
        "ci/linux-cfm-rel",
    ],
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

# RTS builders

try_.builder(
    name = "linux-chromeos-rel-rts",
    mirrors = [
        "ci/linux-chromeos-rel",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.ALWAYS,
        ),
    ),
    builderless = False,
    use_clang_coverage = True,
    coverage_test_types = ["unit", "overall"],
    tryjob = try_.job(
        experiment_percentage = 5,
    ),
)
