# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.chromiumos builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "reclient")
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
)

consoles.list_view(
    name = "tryserver.chromium.chromiumos",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
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

try_.orchestrator_builder(
    name = "chromeos-amd64-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = ["ci/chromeos-amd64-generic-rel"],
    compilator = "chromeos-amd64-generic-rel-compilator",
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "chromeos-amd64-generic-rel-compilator",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    cores = 8,
    main_list_view = "try",
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
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.orchestrator_builder(
    name = "lacros-amd64-generic-rel-orchestrator",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-amd64-generic-rel",
    ],
    compilator = "lacros-amd64-generic-rel-compilator",
    main_list_view = "try",
    use_orchestrator_pool = True,
)

try_.builder(
    name = "lacros-amd64-generic-rel-skylab",
    branch_selector = branches.selector.CROS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_lacros_sdk",
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "amd64-generic",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
        # TODO(https://crbug.com/1399919): change skylab_upload_location
        # as a property. Change try builder as CI mirrors
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chromium-try-skylab",
        ),
    ),
)

try_.compilator_builder(
    name = "lacros-amd64-generic-rel-compilator",
    branch_selector = branches.selector.CROS_BRANCHES,
    cores = None,
    # TODO (crbug.com/1287228): Set correct values once bots are set up
    ssd = None,
    goma_backend = goma.backend.RBE_PROD,
    main_list_view = "try",
)

try_.builder(
    name = "lacros-amd64-generic-rel-skylab-fyi",
    branch_selector = branches.selector.CROS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                "checkout_lacros_sdk",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb", "mb_no_luci_auth"],
            target_bits = 64,
            target_platform = "chromeos",
            target_cros_boards = "eve",
            cros_boards_with_qemu_images = "amd64-generic",
        ),
        build_gs_bucket = "chromium-fyi-archive",
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "gs://lacros-amd64-generic-rel-skylab-try",
        ),
    ),
    builderless = not settings.is_main,
    goma_backend = goma.backend.RBE_PROD,
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
    name = "lacros-arm64-generic-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/lacros-arm64-generic-rel",
    ],
    goma_backend = goma.backend.RBE_PROD,
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
    goma_backend = goma.backend.RBE_PROD,
    main_list_view = "try",
)

try_.builder(
    name = "chromeos-octopus-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    mirrors = [
        "ci/chromeos-octopus-rel",
    ],
    goma_backend = goma.backend.RBE_PROD,
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
    main_list_view = "try",
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
    branch_selector = branches.selector.CROS_BRANCHES,
    mirrors = [
        "ci/linux-lacros-builder-rel",
        "ci/linux-lacros-tester-rel",
    ],
    check_for_flakiness = True,
    compilator = "linux-lacros-rel-compilator",
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux-lacros-rel-compilator",
    branch_selector = branches.selector.CROS_BRANCHES,
    cores = 32,
    main_list_view = "try",
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
