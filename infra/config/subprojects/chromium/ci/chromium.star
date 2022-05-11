# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "sheriff_rotations")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci", "rbe_instance", "rbe_jobs")
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
    # Bump to 32 if needed.
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "dbg",
    ),
    cores = 8,
    execution_timeout = 4 * time.hour,
    tree_closing = True,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "android-archive-rel",
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
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
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
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
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
    name = "linux-archive-dbg",
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "dbg",
    ),
    # Bump to 32 if needed.
    cores = 8,
    tree_closing = True,
    goma_backend = None,
    reclient_jobs = rbe_jobs.HIGH_JOBS_FOR_CI,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-archive-rel",
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
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "linux-archive-tagged",
    builderless = False,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "tag",
    ),
    cores = 32,
    execution_timeout = 7 * time.hour,
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "archive_datas": [
                {
                    "files": [
                        "chrome",
                        "chrome-wrapper",
                        "chrome_100_percent.pak",
                        "chrome_200_percent.pak",
                        "chrome_crashpad_handler",
                        "chrome_sandbox",
                        "icudtl.dat",
                        "libEGL.so",
                        "libGLESv2.so",
                        "libvk_swiftshader.so",
                        "libvulkan.so.1",
                        "MEIPreload/manifest.json",
                        "MEIPreload/preloaded_data.pb",
                        "nacl_helper",
                        "nacl_helper_bootstrap",
                        "nacl_irt_x86_64.nexe",
                        "product_logo_48.png",
                        "resources.pak",
                        "v8_context_snapshot.bin",
                        "vk_swiftshader_icd.json",
                        "xdg-mime",
                        "xdg-settings",
                    ],
                    "dirs": ["ClearKeyCdm", "locales", "resources"],
                    "gcs_bucket": "chromium-browser-versioned",
                    "gcs_path": "experimental/Linux_x64_Tagged/{%chromium_version%}/chrome-linux.zip",
                    "archive_type": "ARCHIVE_TYPE_ZIP",
                },
                {
                    "files": [
                        "chromedriver",
                    ],
                    "gcs_bucket": "chromium-browser-versioned",
                    "gcs_path": "experimental/Linux_x64_Tagged/{%chromium_version%}/chromedriver_linux64.zip",
                    "archive_type": "ARCHIVE_TYPE_ZIP",
                },
            ],
        },
    },
    schedule = "triggered",
    sheriff_rotations = args.ignore_default(None),
    triggered_by = [],
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
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
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
    sheriff_rotations = args.ignore_default(None),
)

ci.builder(
    name = "mac-archive-dbg",
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
    name = "mac-archive-tagged",
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "tag",
    ),
    cores = 12,
    execution_timeout = 7 * time.hour,
    os = os.MAC_DEFAULT,
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "mac-tagged.json",
            ],
        },
    },
    schedule = "triggered",
    sheriff_rotations = args.ignore_default(None),
    triggered_by = [],
)

ci.builder(
    name = "mac-arm64-archive-dbg",
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
    name = "mac-arm64-archive-tagged",
    console_view_entry = consoles.console_view_entry(
        category = "mac|arm",
        short_name = "tag",
    ),
    cores = 12,
    execution_timeout = 7 * time.hour,
    os = os.MAC_DEFAULT,
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "mac-tagged.json",
            ],
        },
    },
    schedule = "triggered",
    sheriff_rotations = args.ignore_default(None),
    triggered_by = [],
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
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "win-archive-rel",
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
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "win-archive-tagged",
    console_view_entry = consoles.console_view_entry(
        category = "win|tag",
        short_name = "64",
    ),
    cores = 32,
    execution_timeout = 7 * time.hour,
    os = os.WINDOWS_DEFAULT,
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "win-tagged.json",
            ],
        },
    },
    schedule = "triggered",
    sheriff_rotations = args.ignore_default(None),
    triggered_by = [],
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
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
    console_view_entry = consoles.console_view_entry(
        category = "win|dbg",
        short_name = "32",
    ),
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "win32-archive-rel",
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
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
)

ci.builder(
    name = "win32-archive-tagged",
    console_view_entry = consoles.console_view_entry(
        category = "win|tag",
        short_name = "32",
    ),
    cores = 32,
    execution_timeout = 7 * time.hour,
    os = os.WINDOWS_DEFAULT,
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "win-tagged.json",
            ],
        },
    },
    schedule = "triggered",
    sheriff_rotations = args.ignore_default(None),
    triggered_by = [],
    goma_backend = None,
    reclient_jobs = rbe_jobs.DEFAULT,
    reclient_instance = rbe_instance.DEFAULT,
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
