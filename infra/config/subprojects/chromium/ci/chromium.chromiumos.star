# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.chromiumos builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "os", "reclient", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.chromiumos",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.modified_default(
        build_time = struct(
            p50_mins = 60,
        ),
    ),
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium.chromiumos",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    ordering = {
        None: ["default"],
        "default": consoles.ordering(short_names = ["ful", "rel"]),
        "simple": ["release", "debug"],
    },
)

ci.builder(
    name = "linux-ash-chromium-generator-rel",
    schedule = "triggered",
    triggered_by = [],
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "default",
    ),
    main_console_view = "main",
    # This builder gets triggered against multiple branches, so it shouldn't be
    # bootstrapped
    bootstrap = False,
    notifies = ["chrome-lacros-engprod-alerts"],
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "cipd_archive_datas": [
                {
                    "yaml_files": [
                        "test_ash_chrome.yaml",
                    ],
                    "refs": [
                        "{%channel%}",
                    ],
                    "tags": {
                        "version": "{%chromium_version%}",
                    },
                    # Because we don't run any tests.
                    "only_set_refs_on_tests_success": False,
                },
            ],
        },
    },
)

ci.builder(
    name = "chromeos-amd64-generic-asan-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
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
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "asn",
    ),
    main_console_view = "main",
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-amd64-generic-cfi-thin-lto-rel",
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
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "cfi",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-sw-engprod@google.com",
    health_spec = health_spec.modified_default(
        build_time = struct(
            p50_mins = 100,
        ),
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-amd64-generic-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "amd64-generic",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|debug|x64",
        short_name = "dbg",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-amd64-generic-lacros-dbg",
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
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "amd64-generic",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "lacros|x64",
        short_name = "dbg",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-amd64-generic-rel-renamed",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a renamed builder of chromeos-amd64-generic-rel.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "checkout_lacros_sdk"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "amd64-generic",
            ],
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "rel",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-amd64-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a compile only builder for Ash chrome." +
                       " This builder also build Lacros with alternative toolchain.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "checkout_lacros_sdk"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "amd64-generic",
            ],
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    # TODO(crbug.com/1471166): enable gardener rotation.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "compile",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "chromeos-amd64-generic-rel-gtest",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a tester builder for Ash chrome." +
                       " This builder only run gtest.",
    triggered_by = ["ci/chromeos-amd64-generic-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos", "checkout_lacros_sdk"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "amd64-generic",
            ],
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    # TODO(crbug.com/1471166): enable gardener rotation.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "compile",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-arm-generic-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "arm-generic",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|debug",
        short_name = "arm",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-sw-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-arm-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["arm-generic"],
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "arm",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-arm64-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["arm64-generic"],
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "a64",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-sw-engprod@google.com",
)

ci.builder(
    name = "chromeos-jacuzzi-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "arm",
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "jacuzzi",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    # TODO(crbug.com/1342987): Add to the sheriff rotation if/when the builder
    # is stable.
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "jcz",
    ),
    main_console_view = "main",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-octopus-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
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
                "octopus",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    # TODO(crbug.com/1342987): Add to the sheriff rotation if/when the builder
    # is stable.
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "oct",
    ),
    main_console_view = "main",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-amd64-generic-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_no_telemetry_dependencies",
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
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chromium-ci-skylab",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "lacros|x64",
        short_name = "skylab",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-amd64-generic-rel-non-skylab",
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
                "eve",
            ],
            cros_boards_with_qemu_images = [
                "amd64-generic",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "lacros|x64",
        short_name = "rel",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-arm-generic-rel-skylab",
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
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = "arm-generic",
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chromium-ci-skylab",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros|arm",
        short_name = "sky",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-arm64-generic-rel-skylab",
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
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = "arm64-generic",
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
        skylab_upload_location = builder_config.skylab_upload_location(
            gs_bucket = "chromium-ci-skylab",
        ),
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros|arm64",
        short_name = "sky",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-arm-generic-rel",
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
                "arm-generic",
                "jacuzzi",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    # TODO(crbug.com/1202631) Enable tree closing when stable.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "lacros|arm",
        short_name = "arm",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-arm64-generic-rel",
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
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "arm64-generic",
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    # TODO(https://crbug.com/1342761): enable sheriff rotation and tree_closing
    # when the builder is stable.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "lacros|arm64",
        short_name = "arm64",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-chromeos-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "dbg",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    health_spec = health_spec.modified_default(
        build_time = struct(
            p50_mins = 150,
        ),
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-chromeos-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
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
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    # See crbug.com/1345687. This builder need higher memory.
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "rel",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-lacros-builder-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium_no_telemetry_dependencies",
            apply_configs = [
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
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    # See crbug.com/1345687. This builder need higher memory.
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "lcr",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "linux-lacros-tester-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    triggered_by = ["linux-lacros-builder-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium_no_telemetry_dependencies",
            apply_configs = [
                "use_clang_coverage",
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
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "lcr",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
)

ci.builder(
    name = "linux-lacros-dbg",
    branch_selector = branches.selector.CROS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    # See crbug.com/1345687. This builder need higher memory.
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "debug",
        short_name = "lcr",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

# For Chromebox for meetings(CfM)
ci.builder(
    name = "linux-cfm-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
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
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "default|cfm",
        short_name = "cfm",
    ),
    main_console_view = "main",
    contact_team_email = "core-devices-eng@google.com",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)
