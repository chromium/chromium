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
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium",
    pool = ci.DEFAULT_POOL,
    os = os.LINUX_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    main_console_view = "main",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
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
    name = "android-archive-dbg",
    cores = 8,
    tree_closing = True,
    # Bump to 32 if needed.
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "dbg",
    ),
    execution_timeout = 4 * time.hour,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-archive-rel",
    cores = 32,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "rel",
    ),
    goma_backend = None,
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
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-official",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builderless = False,
    cores = 32,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "off",
    ),
    # See https://crbug.com/1153349#c22, as we update symbol_level=2, build
    # needs longer time to complete.
    execution_timeout = 7 * time.hour,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "fuchsia-official",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
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
            category = "ci",
            short_name = "off-x64",
        ),
    ],
    # TODO: Change this back down to something reasonable once these builders
    # have populated their cached by getting through the compile step
    execution_timeout = 10 * time.hour,
)

ci.builder(
    name = "linux-archive-dbg",
    # Bump to 32 if needed.
    cores = 8,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "dbg",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-archive-rel",
    cores = 32,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "rel",
    ),
    goma_backend = None,
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
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-archive-tagged",
    schedule = "triggered",
    triggered_by = [],
    builderless = False,
    cores = 32,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "tag",
    ),
    execution_timeout = 7 * time.hour,
    goma_backend = None,
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
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "linux-official",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builderless = False,
    cores = 32,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "off",
    ),
    execution_timeout = 7 * time.hour,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "mac-archive-dbg",
    # Bump to 8 cores if needed.
    cores = 4,
    os = os.MAC_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "mac-archive-rel",
    cores = 12,
    os = os.MAC_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "rel",
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
    name = "mac-archive-tagged",
    schedule = "triggered",
    triggered_by = [],
    cores = 12,
    os = os.MAC_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "tag",
    ),
    execution_timeout = 7 * time.hour,
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
)

ci.builder(
    name = "mac-arm64-archive-dbg",
    cores = 12,
    os = os.MAC_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac|arm",
        short_name = "dbg",
    ),
)

ci.builder(
    name = "mac-arm64-archive-rel",
    cores = 12,
    os = os.MAC_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "mac|arm",
        short_name = "rel",
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
    name = "mac-arm64-archive-tagged",
    schedule = "triggered",
    triggered_by = [],
    cores = 12,
    os = os.MAC_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "mac|arm",
        short_name = "tag",
    ),
    execution_timeout = 7 * time.hour,
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
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "off",
    ),
    # TODO(crbug.com/1279290) builds with PGO change take long time.
    # Keep in sync with mac-official in try/chromium.star.
    execution_timeout = 7 * time.hour,
)

ci.builder(
    name = "win-archive-dbg",
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "win|dbg",
        short_name = "64",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "win-archive-rel",
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "64",
    ),
    goma_backend = None,
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
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "win-archive-tagged",
    schedule = "triggered",
    triggered_by = [],
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "win|tag",
        short_name = "64",
    ),
    execution_timeout = 7 * time.hour,
    goma_backend = None,
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
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
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
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win|off",
        short_name = "64",
    ),
    # TODO(crbug.com/1155416) builds with PGO change take long time.
    execution_timeout = 7 * time.hour,
)

ci.builder(
    name = "win32-archive-dbg",
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "win|dbg",
        short_name = "32",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "win32-archive-rel",
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    tree_closing = True,
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "32",
    ),
    goma_backend = None,
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
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "win32-archive-tagged",
    schedule = "triggered",
    triggered_by = [],
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "win|tag",
        short_name = "32",
    ),
    execution_timeout = 7 * time.hour,
    goma_backend = None,
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
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
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
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win|off",
        short_name = "32",
    ),
    # TODO(crbug.com/1155416) builds with PGO change take long time.
    execution_timeout = 7 * time.hour,
)
