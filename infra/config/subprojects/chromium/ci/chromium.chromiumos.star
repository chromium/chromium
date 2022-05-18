# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.chromiumos builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "sheriff_rotations")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.chromiumos",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,
)

consoles.console_view(
    name = "chromium.chromiumos",
    branch_selector = branches.CROS_LTS_MILESTONE,
    ordering = {
        None: ["default"],
        "default": consoles.ordering(short_names = ["ful", "rel"]),
        "simple": ["release", "debug"],
    },
)

ci.builder(
    name = "linux-ash-chromium-generator-rel",
    # This builder gets triggered against multiple branches, so it shouldn't be
    # bootstrapped
    bootstrap = False,
    console_view_entry = consoles.console_view_entry(
        category = "default",
    ),
    tree_closing = False,
    main_console_view = "main",
    notifies = ["chrome-lacros-engprod-alerts"],
    triggered_by = [],
    schedule = "triggered",
    sheriff_rotations = args.ignore_default(None),
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
    name = "Linux ChromiumOS Full",
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "ful",
    ),
    main_console_view = "main",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "linux-chromiumos-full.json",
            ],
        },
    },
)

ci.builder(
    name = "Linux ChromiumOS Full (reclient shadow)",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "ful",
    ),
    main_console_view = "main",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "linux-chromiumos-full.json",
            ],
        },
    },
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
)

ci.builder(
    name = "chromeos-amd64-generic-asan-rel",
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "asn",
    ),
    main_console_view = "main",
)

ci.builder(
    name = "chromeos-amd64-generic-asan-rel (reclient shadow)",
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "asn",
    ),
    main_console_view = "main",
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
            target_cros_boards = [
                "amd64-generic",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
)

ci.builder(
    name = "chromeos-amd64-generic-cfi-thin-lto-rel",
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "cfi",
    ),
    main_console_view = "main",
)

ci.builder(
    name = "chromeos-amd64-generic-dbg",
    branch_selector = branches.STANDARD_MILESTONE,
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
            target_cros_boards = [
                "amd64-generic",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|debug|x64",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
)

ci.builder(
    name = "chromeos-amd64-generic-dbg (reclient shadow)",
    console_view_entry = consoles.console_view_entry(
        category = "simple|debug|x64",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
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
            target_cros_boards = [
                "amd64-generic",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
)

ci.builder(
    name = "chromeos-amd64-generic-lacros-dbg",
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
            target_cros_boards = [
                "amd64-generic",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "lacros|x64",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
)

ci.builder(
    name = "chromeos-amd64-generic-rel",
    branch_selector = branches.CROS_LTS_MILESTONE,
    builder_spec = builder_config.builder_spec(
        build_gs_bucket = "chromium-chromiumos-archive",
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            cros_boards_with_qemu_images = "amd64-generic-vm",
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release|x64",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
)

ci.builder(
    name = "chromeos-arm-generic-dbg",
    console_view_entry = consoles.console_view_entry(
        category = "simple|debug",
        short_name = "arm",
    ),
    main_console_view = "main",
)

ci.builder(
    name = "chromeos-arm-generic-dbg (reclient shadow)",
    console_view_entry = consoles.console_view_entry(
        category = "simple|debug",
        short_name = "arm",
    ),
    main_console_view = "main",
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
            target_cros_boards = [
                "arm-generic",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
)

ci.builder(
    name = "chromeos-arm-generic-rel",
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["arm-generic"],
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
    ),
    branch_selector = branches.CROS_LTS_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "arm",
    ),
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
)

ci.builder(
    name = "chromeos-arm64-generic-rel",
    builder_spec = builder_config.builder_spec(
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = ["arm64-generic"],
        ),
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = ["chromeos"],
        ),
    ),
    branch_selector = branches.CROS_LTS_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "a64",
    ),
    main_console_view = "main",
)

ci.builder(
    name = "chromeos-kevin-rel",
    branch_selector = branches.CROS_LTS_MILESTONE,
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
            target_cros_boards = [
                "kevin",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "kvn",
    ),
    main_console_view = "main",
)

ci.builder(
    name = "chromeos-kevin-rel (reclient shadow)",
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "kvn",
    ),
    main_console_view = "main",
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
            target_cros_boards = [
                "kevin",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
)

ci.builder(
    name = "lacros-amd64-generic-binary-size-rel",
    console_view_entry = consoles.console_view_entry(
        category = "lacros|size",
    ),
    main_console_view = "main",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "archive_datas": [
                # The list of files and dirs should be synched with
                # _TRACKED_ITEMS in //build/lacros/lacros_resource_sizes.py.
                {
                    "files": [
                        "chrome",
                        "chrome_100_percent.pak",
                        "chrome_200_percent.pak",
                        "chrome_crashpad_handler",
                        "headless_lib_data.pak",
                        "headless_lib_strings.pak",
                        "icudtl.dat",
                        "nacl_helper",
                        "nacl_irt_x86_64.nexe",
                        "resources.pak",
                        "snapshot_blob.bin",
                    ],
                    "dirs": ["locales"],
                    "gcs_bucket": "chromium-lacros-fishfood",
                    "gcs_path": "x86_64/{%position%}/lacros.zip",
                    "archive_type": "ARCHIVE_TYPE_ZIP",
                },
            ],
        },
    },
)

ci.builder(
    name = "lacros-amd64-generic-rel",
    branch_selector = branches.STANDARD_MILESTONE,
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
            target_cros_boards = [
                "eve",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
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
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
)

ci.builder(
    name = "lacros-arm-generic-rel",
    branch_selector = branches.STANDARD_MILESTONE,
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
            target_cros_boards = [
                "arm-generic",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "lacros|arm",
        short_name = "arm",
    ),
    # TODO(crbug.com/1202631) Enable tree closing when stable.
    tree_closing = False,
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
)

ci.builder(
    name = "linux-chromeos-dbg",
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
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "dbg",
    ),
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
)

ci.builder(
    name = "linux-chromeos-rel",
    branch_selector = branches.CROS_LTS_MILESTONE,
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
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "rel",
    ),
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
)

ci.builder(
    name = "linux-lacros-builder-rel",
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
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "lcr",
    ),
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
)

ci.builder(
    name = "linux-lacros-tester-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
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
    branch_selector = branches.STANDARD_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "default",
        short_name = "lcr",
    ),
    main_console_view = "main",
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["linux-lacros-builder-rel"],
    tree_closing = False,
)

ci.builder(
    name = "linux-lacros-dbg",
    branch_selector = branches.STANDARD_MILESTONE,
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
        category = "debug",
        short_name = "lcr",
    ),
    cq_mirrors_console_view = "mirrors",
    main_console_view = "main",
)

# For Chromebox for meetings(CfM)
ci.builder(
    name = "linux-cfm-rel",
    console_view_entry = consoles.console_view_entry(
        category = "simple|release",
        short_name = "cfm",
    ),
    main_console_view = "main",
)
