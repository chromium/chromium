# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "cpu", "os", "reclient", "sheriff_rotations")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

# Take care when changing the GN args of any of these builders to ensure that
# you do not include a configuration with 'chrome_with_codecs' since these
# builders generate publicly advertised non-Official builds which are not
# allowed to have proprietary codecs enabled.

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium",
    pool = ci.DEFAULT_POOL,
    os = os.LINUX_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    main_console_view = "main",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
)

consoles.console_view(
    name = "chromium",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
        branches.selector.DESKTOP_BRANCHES,
        branches.selector.FUCHSIA_BRANCHES,
    ],
    ordering = {
        "*type*": consoles.ordering(short_names = ["dbg", "rel", "off"]),
        "android": "*type*",
        "fuchsia": "*type*",
        "linux": "*type*",
        "mac": "*type*",
        "win": "*type*",
    },
    include_experimental_builds = True,
)

ci.builder(
    name = "android-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    cores = 32,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "rel",
    ),
    contact_team_email = "clank-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "release_builder",
            "reclient",
            "minimal_symbols",
            "strip_debug_info",
        ],
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "android-archive-rel.json",
            ],
        },
    },
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-arm64-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    cores = 32,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "android|arm",
        short_name = "arm64",
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "release_builder",
            "reclient",
            "minimal_symbols",
            "strip_debug_info",
            "arm64",
        ],
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "android-arm64-archive-rel.json",
            ],
        },
    },
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-official",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            target_arch = builder_config.target_arch.ARM,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    builderless = False,
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "off",
    ),
    contact_team_email = "clank-engprod@google.com",
    # See https://crbug.com/1153349#c22, as we update symbol_level=2, build
    # needs longer time to complete.
    execution_timeout = 7 * time.hour,
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "reclient",
            "android_builder_without_codecs",
            "full_symbols",
        ],
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "fuchsia-official",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "fuchsia_x64",
                "checkout_pgo_profiles",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_bits = 64,
            target_platform = builder_config.target_platform.FUCHSIA,
        ),
    ),
    builderless = False,
    cores = 32,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia",
            short_name = "off",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.fuchsia",
            category = "gardener|ci|x64",
            short_name = "off",
        ),
    ],
    # TODO: Change this back down to something reasonable once these builders
    # have populated their cached by getting through the compile step
    execution_timeout = 10 * time.hour,
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "reclient",
            "fuchsia",
        ],
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-chromeos-archive-rel",
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
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    cores = 8,
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "cros",
        short_name = "lnx",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_with_codecs",
            "release_builder",
            "reclient",
            "use_cups",
        ],
    ),
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
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-lacros-archive-rel",
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
        ),
    ),
    cores = 8,
    # TODO(crbug.com/1362019): Turn on when stable.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "lnx",
    ),
    gn_args = gn_args.config(
        configs = [
            "lacros_on_linux",
            "release_builder",
            "reclient",
            "also_build_ash_chrome",
        ],
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "linux-lacros-archive-rel.json",
            ],
        },
    },
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros64-archive-rel",
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
    ),
    cores = 32,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "rel",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "reclient",
            "amd64-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
        ],
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "lacros64-archive-rel.json",
            ],
        },
    },
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-arm-archive-rel",
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
                "arm-generic",
            ],
        ),
    ),
    cores = 32,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "arm",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "reclient",
            "arm-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
        ],
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "lacros-arm-archive-rel.json",
            ],
        },
    },
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "lacros-arm64-archive-rel",
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
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
            target_cros_boards = [
                "arm64-generic",
            ],
        ),
    ),
    cores = 32,
    sheriff_rotations = args.ignore_default(None),
    # TODO(crbug.com/1363272): Enable tree_closing/sheriff when stable.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "arm64",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos_device",
            "dcheck_off",
            "reclient",
            "arm64-generic-crostoolchain",
            "ozone_headless",
            "lacros",
            "release",
        ],
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "lacros-arm64-archive-rel.json",
            ],
        },
    },
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    cores = 32,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "rel",
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "updater",
        ],
    ),
    notifies = ["linux-archive-rel"],
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "linux-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "linux-official",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_bits = 64,
        ),
    ),
    builderless = False,
    cores = 32,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "off",
    ),
    execution_timeout = 7 * time.hour,
    gn_args = gn_args.config(
        configs = ["official_optimize", "reclient"],
    ),
    health_spec = health_spec.modified_default({
        "Unhealthy": struct(
            build_time = struct(
                p50_mins = 240,
            ),
        ),
    }),
)

ci.builder(
    name = "mac-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    cores = 12,
    os = os.MAC_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "rel",
    ),
    contact_team_email = "bling-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "mac_strip",
            "minimal_symbols",
        ],
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "mac-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "mac-arm64-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    cores = 12,
    os = os.MAC_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac|arm",
        short_name = "rel",
    ),
    contact_team_email = "bling-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "mac_strip",
            "minimal_symbols",
            "arm64",
        ],
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "mac-arm64-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "mac-official",
    branch_selector = branches.selector.MAC_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_bits = 64,
        ),
    ),
    builderless = False,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "off",
    ),
    contact_team_email = "bling-engprod@google.com",
    # TODO(crbug.com/1279290) builds with PGO change take long time.
    # Keep in sync with mac-official in try/chromium.star.
    execution_timeout = 15 * time.hour,
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "reclient",
        ],
    ),
)

ci.builder(
    name = "win-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    builderless = False,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "64",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "minimal_symbols",
        ],
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "win-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "win-official",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_bits = 64,
        ),
    ),
    builderless = False,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win|off",
        short_name = "64",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    # TODO(crbug.com/1155416) builds with PGO change take long time.
    execution_timeout = 7 * time.hour,
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "reclient",
            "minimal_symbols",
        ],
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "win32-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
        ),
    ),
    builderless = False,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "32",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "reclient",
            "x86",
            "minimal_symbols",
        ],
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "win32-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "win32-official",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_bits = 32,
        ),
    ),
    builderless = False,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win|off",
        short_name = "32",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    # TODO(crbug.com/1155416) builds with PGO change take long time.
    execution_timeout = 7 * time.hour,
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "reclient",
            "x86",
        ],
    ),
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)
