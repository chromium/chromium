# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "reclient", "sheriff_rotations")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium",
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    main_console_view = "main",
    os = os.LINUX_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
)

consoles.console_view(
    name = "chromium",
    branch_selector = [
        branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
        branches.FUCHSIA_LTS_MILESTONE,
    ],
    include_experimental_builds = True,
    ordering = {
        "*type*": consoles.ordering(short_names = ["dbg", "rel", "off"]),
        "android": "*type*",
        "fuchsia": "*type*",
        "linux": "*type*",
        "mac": "*type*",
        "win": "*type*",
    },
)

ci.builder(
    name = "android-archive-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_platform = builder_config.target_platform.ANDROID,
            target_arch = builder_config.target_arch.ARM,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    # Bump to 32 if needed.
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "dbg",
    ),
    cores = 8,
    execution_timeout = 4 * time.hour,
    tree_closing = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "android-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_platform = builder_config.target_platform.ANDROID,
            target_arch = builder_config.target_arch.ARM,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "rel",
    ),
    cores = 32,
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
    tree_closing = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "android-official",
    branch_selector = branches.STANDARD_MILESTONE,
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "off",
    ),
    cores = 32,
    # See https://crbug.com/1153349#c22, as we update symbol_level=2, build
    # needs longer time to complete.
    execution_timeout = 7 * time.hour,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    sheriff_rotations = args.ignore_default(None),
)

ci.builder(
    name = "fuchsia-official",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    builderless = False,
    console_view_entry = [
        consoles.console_view_entry(
            category = "fuchsia",
            short_name = "off",
        ),
        consoles.console_view_entry(
            branch_selector = branches.MAIN,
            console_view = "sheriff.fuchsia",
            category = "ci",
            short_name = "off-x64",
        ),
    ],
    cores = 32,
    # TODO: Change this back down to something reasonable once these builders
    # have populated their cached by getting through the compile step
    execution_timeout = 10 * time.hour,
    sheriff_rotations = args.ignore_default(None),
)

ci.builder(
    name = "lacros64-archive-rel",
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "rel",
    ),
    branch_selector = branches.STANDARD_MILESTONE,
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
            target_cros_boards = [
                "amd64-generic",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    cores = 32,
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
    tree_closing = True,
)

ci.builder(
    name = "lacros-arm-archive-rel",
    console_view_entry = consoles.console_view_entry(
        category = "lacros",
        short_name = "arm",
    ),
    branch_selector = branches.STANDARD_MILESTONE,
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
            target_cros_boards = [
                "arm-generic",
            ],
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
    ),
    cores = 32,
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
    tree_closing = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-archive-dbg",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "enable_reclient",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "dbg",
    ),
    # Bump to 32 if needed.
    cores = 8,
    tree_closing = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "enable_reclient",
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
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "rel",
    ),
    cores = 32,
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
    tree_closing = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "linux-official",
    branch_selector = branches.STANDARD_MILESTONE,
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "off",
    ),
    cores = 32,
    execution_timeout = 7 * time.hour,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    sheriff_rotations = args.ignore_default(None),
)

ci.builder(
    name = "mac-archive-dbg",
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "dbg",
    ),
    # Bump to 8 cores if needed.
    cores = 4,
    os = os.MAC_DEFAULT,
    tree_closing = True,
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
                "goma_use_local",  # to mitigate compile step timeout (crbug.com/1056935).
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "rel",
    ),
    cores = 12,
    os = os.MAC_DEFAULT,
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
    tree_closing = True,
)

ci.builder(
    name = "mac-arm64-archive-dbg",
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "mac|arm",
        short_name = "dbg",
    ),
    cores = 12,
    os = os.MAC_DEFAULT,
    tree_closing = True,
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
                "goma_use_local",  # to mitigate compile step timeout (crbug.com/1056935).
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "mac|arm",
        short_name = "rel",
    ),
    cores = 12,
    os = os.MAC_DEFAULT,
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
    tree_closing = True,
)

ci.builder(
    name = "mac-official",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
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
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "off",
    ),
    # TODO(crbug.com/1279290) builds with PGO change take long time.
    # Keep in sync with mac-official in try/chromium.star.
    execution_timeout = 7 * time.hour,
    os = os.MAC_ANY,
)

ci.builder(
    name = "win-archive-dbg",
    console_view_entry = consoles.console_view_entry(
        category = "win|dbg",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
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
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
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
    tree_closing = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "win-official",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
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
    console_view_entry = consoles.console_view_entry(
        category = "win|off",
        short_name = "64",
    ),
    cores = 32,
    # TODO(crbug.com/1155416) builds with PGO change take long time.
    execution_timeout = 7 * time.hour,
    os = os.WINDOWS_DEFAULT,
)

ci.builder(
    name = "win32-archive-dbg",
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
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "win|dbg",
        short_name = "32",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
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
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "32",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
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
    tree_closing = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.builder(
    name = "win32-official",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "win|off",
        short_name = "32",
    ),
    cores = 32,
    # TODO(crbug.com/1155416) builds with PGO change take long time.
    execution_timeout = 7 * time.hour,
    os = os.WINDOWS_DEFAULT,
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
)
