# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.chromiumos builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "gardener_rotations", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.chromiumos",
    builder_config_settings = builder_config.ci_settings(
        retry_failed_shards = True,
    ),
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    tree_closing = True,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    health_spec = health_spec.modified_default({
        "Unhealthy": struct(
            build_time = struct(
                p50_mins = 60,
            ),
        ),
    }),
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "default",
    ),
    main_console_view = "main",
    # This builder gets triggered against multiple branches, so it shouldn't be
    # bootstrapped
    bootstrap = False,
    gardener_rotations = args.ignore_default(None),
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic",
            "ozone_headless",
            "asan",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "asn",
    ),
    main_console_view = "main",
    contact_team_email = "chrome-sanitizer-builder-owners@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic",
            "ozone_headless",
            "cfi_full",
            "thin_lto",
            "also_build_lacros_chrome_for_architecture_amd64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "cfi",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-sw-engprod@google.com",
    health_spec = health_spec.modified_default({
        "Unhealthy": struct(
            build_time = struct(
                p50_mins = 100,
            ),
        ),
    }),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic",
            "ozone_headless",
            "debug",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|debug|x64",
        short_name = "dbg",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "debug",
            "static",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "lacros|x64",
        short_name = "dbg",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-amd64-generic-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a compile only builder for Ash chrome." +
                       " This builder also build Lacros with alternative toolchain.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
                # This is necessary due to a child builder running the
                # telemetry_perf_unittests suite.
                "chromium_with_telemetry_dependencies",
                "checkout_lacros_sdk",
            ],
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic-vm",
            "ozone_headless",
            "use_fake_dbus_clients",
            "also_build_lacros_chrome_for_architecture_amd64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "compile",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "gtest",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "chromeos-amd64-generic-rel-tast",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = "This is a tester builder for Ash chrome." +
                       " This builder only run tast tests. If you see" +
                       " test failures, please contact ChromeOS gardeners" +
                       " for help.",
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
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "tast",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    # Tast tests should be monitored by CrOS gardeners, not Chromium gardeners.
    gardener_rotations = args.ignore_default(gardener_rotations.CHROMIUMOS),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "arm-generic",
            "debug",
            "ozone_headless",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|debug",
        short_name = "arm",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-sw-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "arm-generic",
            "ozone_headless",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "arm",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            cros_boards_with_qemu_images = ["arm64-generic-vm"],
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "remoteexec",
            "arm64-generic-vm",
            "dcheck_always_on",
            "ozone_headless",
        ],
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
    description_html = """\
This builder builds chromium and tests it on the public CrOS image on skylab DUTs.
""",
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
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "jacuzzi",
                # `arm64-generic` is necessary for lacros build.
                "arm64-generic",
            ],
        ),
        skylab_upload_location = builder_config.skylab_upload_location(
            # Both CI and try use the same `chromium-skylab-try` bucket.
            gs_bucket = "chromium-skylab-try",
            gs_extra = "ash",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "also_build_lacros_chrome_for_architecture_arm64",
            "chromeos_device",
            "dcheck_off",
            "include_unwind_tables",
            "is_skylab",
            "jacuzzi",
            "ozone_headless",
            "remoteexec",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "jcz",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-velocity@google.com",
    # Tast tests should be monitored by CrOS gardeners, not Chromium gardeners.
    gardener_rotations = args.ignore_default(gardener_rotations.CHROMIUMOS),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "chromeos-octopus-rel",
    branch_selector = branches.selector.CROS_LTS_BRANCHES,
    description_html = """\
This builder builds chromium and tests it on the public CrOS image on skylab DUTs.
""",
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
                "octopus",
                # `amd64-generic` is necessary for lacros build.
                "amd64-generic",
            ],
        ),
        skylab_upload_location = builder_config.skylab_upload_location(
            # Both CI and try use the same `chromium-skylab-try` bucket.
            gs_bucket = "chromium-skylab-try",
            gs_extra = "ash",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "also_build_lacros_chrome_for_architecture_amd64",
            "chromeos_device",
            "dcheck_off",
            "include_unwind_tables",
            "is_skylab",
            "octopus",
            "ozone_headless",
            "remoteexec",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "oct",
    ),
    main_console_view = "main",
    contact_team_email = "chromeos-velocity@google.com",
    # Tast tests should be monitored by CrOS gardeners, not Chromium gardeners.
    gardener_rotations = args.ignore_default(gardener_rotations.CHROMIUMOS),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-amd64-generic-rel",
    branch_selector = branches.selector.CROS_BRANCHES,
    description_html = "This is a compile only builder for Lacros chrome.",
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
            "is_skylab",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "lacros|x64",
        short_name = "skylab",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "lacros-amd64-generic-rel-gtest",
    branch_selector = branches.selector.CROS_BRANCHES,
    description_html = "This is a tester builder for Lacros chrome." +
                       " This builder only run gtest.",
    triggered_by = ["ci/lacros-amd64-generic-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
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
        short_name = "gtest",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "lacros-amd64-generic-rel-tast",
    branch_selector = branches.selector.CROS_BRANCHES,
    description_html = "This is a tester builder for Lacros chrome." +
                       " This builder only run tast tests. If you see" +
                       " test failures, please contact ChromeOS gardeners" +
                       " for help.",
    triggered_by = ["ci/lacros-amd64-generic-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
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
        short_name = "tast",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    # Tast tests should be monitored by CrOS gardeners, not Chromium gardeners.
    gardener_rotations = args.ignore_default(gardener_rotations.CHROMIUMOS),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "amd64-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "lacros|x64",
        short_name = "rel",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    # Tast tests should be monitored by CrOS gardeners, not Chromium gardeners.
    gardener_rotations = args.ignore_default(gardener_rotations.CHROMIUMOS),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "arm-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
            "is_skylab",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros|arm",
        short_name = "sky",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "arm64-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
            "is_skylab",
        ],
    ),
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "lacros|arm64",
        short_name = "sky",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    # Tast tests should be monitored by CrOS gardeners, not Chromium gardeners.
    gardener_rotations = args.ignore_default(gardener_rotations.CHROMIUMOS),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            ],
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "arm-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
        ],
    ),
    # TODO(crbug.com/40179221) Enable tree closing when stable.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "lacros|arm",
        short_name = "arm",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chrome-desktop-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "remoteexec",
            "arm64-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
        ],
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "lacros|arm64",
        short_name = "arm64",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    # TODO(crbug.com/40231151): enable gardener rotation and tree_closing
    # when the builder is stable.
    gardener_rotations = args.ignore_default(None),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_with_codecs",
            "debug_builder",
            "remoteexec",
            "use_cups",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "dbg",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    contact_team_email = "chromeos-sw-engprod@google.com",
    health_spec = health_spec.modified_default({
        "Unhealthy": health_spec.unhealthy_thresholds(
            build_time = struct(
                p50_mins = 150,
            ),
        ),
    }),
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_with_codecs",
            "release_builder",
            "remoteexec",
            "use_cups",
            "also_build_lacros_chrome",
        ],
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
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "release_builder",
            "remoteexec",
            "also_build_ash_chrome",
            "use_cups",
        ],
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
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            target_platform = builder_config.target_platform.CHROMEOS,
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "debug_builder",
            "remoteexec",
            "also_build_ash_chrome",
            "use_cups",
        ],
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
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
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
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "cfm",
            "release_builder",
            "remoteexec",
            "chromeos",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "default|cfm",
        short_name = "cfm",
    ),
    main_console_view = "main",
    contact_team_email = "core-devices-eng@google.com",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)
