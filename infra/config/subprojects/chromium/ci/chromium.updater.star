# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.updater builder group."""

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "builders", "cpu", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify")
load("@chromium-luci//targets.star", "targets")
load("//lib/ci_constants.star", "ci_constants")
load("//lib/siso.star", "siso")

ci.defaults.set(
    executable = ci_constants.DEFAULT_EXECUTABLE,
    builder_group = "chromium.updater",
    pool = ci_constants.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    health_spec = health_spec.default(),
    service_account = ci_constants.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci_constants.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
)

consoles.console_view(
    name = "chromium.updater",
)

# The chromium.updater console includes some entries from official chrome builders.
[branches.console_view_entry(
    console_view = "chromium.updater",
    builder = "chrome:official/{}".format(name),
    category = category,
    short_name = short_name,
) for name, category, short_name in (
    ("mac64", "official|mac", "64"),
    ("mac-arm64", "official|mac", "arm64"),
    ("win-asan", "official|win", "asan"),
    ("win-clang", "official|win", "clang"),
    ("win64-clang", "official|win", "clang (64)"),
)]

_UPDATER_LINK = linkify("https://chromium.googlesource.com/chromium/src/+/main/docs/updater/design_doc.md", "Chromium updater")

ci.builder(
    name = "linux-updater-builder-dbg",
    description_html = _UPDATER_LINK + " Linux x64 debug builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "debug_static_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "debug|linux",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.builder(
    name = "linux-updater-builder-rel",
    description_html = _UPDATER_LINK + " Linux x64 release builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "release_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    builderless = True,
    os = os.LINUX_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|linux",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "linux-updater-tester-dbg",
    description_html = _UPDATER_LINK + " Linux x64 debug builder.",
    parent = "linux-updater-builder-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_linux",
        ],
        mixins = [
            "linux-jammy",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|linux",
        short_name = "test",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "linux-updater-tester-rel",
    description_html = _UPDATER_LINK + " Linux x64 release tester.",
    parent = "linux-updater-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_linux",
        ],
        mixins = [
            "linux-jammy",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|linux",
        short_name = "test",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.builder(
    name = "mac-updater-builder-dbg",
    description_html = _UPDATER_LINK + " macOS x64 debug builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "debug_static_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac (x64)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.builder(
    name = "mac-updater-builder-rel",
    description_html = _UPDATER_LINK + " macOS x64 release builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "release_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "release|mac (x64)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.builder(
    name = "mac-updater-builder-arm64-dbg",
    description_html = _UPDATER_LINK + " macOS arm64 debug builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "updater",
            "debug_static_builder",
            "remoteexec",
            "mac",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac (arm64)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.builder(
    name = "mac-updater-builder-arm64-rel",
    description_html = _UPDATER_LINK + " macOS arm64 release builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "arm64",
            "updater",
            "release_builder",
            "remoteexec",
            "mac",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "release|mac (arm64)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.builder(
    name = "mac-updater-builder-asan-dbg",
    description_html = _UPDATER_LINK + " macOS x64 ASAN debug builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "asan",
            "debug_static_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac (x64)",
        short_name = "bld-asan",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "mac12-arm64-updater-tester-rel",
    description_html = _UPDATER_LINK + " macOS 12 arm64 release tester.",
    parent = "mac-updater-builder-arm64-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_mac",
        ],
        mixins = [
            "mac_12_arm64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|mac (arm64)",
        short_name = "12",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "mac12-x64-updater-tester-asan-dbg",
    description_html = _UPDATER_LINK + " macOS 12 x64 ASAN debug tester.",
    parent = "mac-updater-builder-asan-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_mac",
        ],
        mixins = [
            "mac_12_x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac (x64)",
        short_name = "12 asan",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "mac13-arm64-updater-tester-dbg",
    description_html = _UPDATER_LINK + " macOS 13 arm64 debug tester.",
    parent = "mac-updater-builder-arm64-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_mac",
        ],
        mixins = [
            "mac_13_arm64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac (arm64)",
        short_name = "13",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "mac13-x64-updater-tester-rel",
    description_html = _UPDATER_LINK + " macOS 13 x64 release tester.",
    parent = "mac-updater-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_mac",
        ],
        mixins = [
            "mac_13_x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|mac (x64)",
        short_name = "13",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "mac14-arm64-updater-tester-dbg",
    description_html = _UPDATER_LINK + " macOS 14 arm64 debug tester.",
    parent = "mac-updater-builder-arm64-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_user_gtests_mac",
        ],
        mixins = [
            "mac_14_arm64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac (arm64)",
        short_name = "14",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "mac14-x64-updater-tester-rel",
    description_html = _UPDATER_LINK + " macOS 14 x64 release tester.",
    parent = "mac-updater-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_user_gtests_mac",
        ],
        mixins = [
            "mac_14_x64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|mac (x64)",
        short_name = "14",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "mac15-arm64-updater-tester-dbg",
    description_html = _UPDATER_LINK + " macOS 15 arm64 debug tester.",
    parent = "mac-updater-builder-arm64-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_user_gtests_mac",
        ],
        mixins = [
            "mac_15_arm64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac (arm64)",
        short_name = "15",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "mac15-arm64-updater-tester-rel",
    description_html = _UPDATER_LINK + " macOS 15 arm64 release tester.",
    parent = "mac-updater-builder-arm64-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_user_gtests_mac",
        ],
        mixins = [
            "mac_15_arm64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|mac (arm64)",
        short_name = "15",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.builder(
    name = "win-arm64-updater-builder-dbg",
    description_html = _UPDATER_LINK + " Windows arm64 debug builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "debug_static_builder",
            "remoteexec",
            "win",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (arm64)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
    execution_timeout = 6 * time.hour,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-arm64-updater-builder-rel",
    description_html = _UPDATER_LINK + " Windows arm64 release builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "release_builder",
            "remoteexec",
            "win",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (arm64)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
    execution_timeout = 6 * time.hour,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-updater-builder-dbg",
    description_html = _UPDATER_LINK + " Windows x64 debug builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "debug_static_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win32-updater-builder-dbg",
    description_html = _UPDATER_LINK + " Windows x32 debug builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "debug_static_builder",
            "remoteexec",
            "x86",
            "no_symbols",
            "win",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (32)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
    execution_timeout = ci_constants.DEFAULT_EXECUTION_TIMEOUT * 2,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-updater-builder-rel",
    description_html = _UPDATER_LINK + " Windows x64 release builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "release_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win32-updater-builder-rel",
    description_html = _UPDATER_LINK + " Windows x32 release builder.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "updater",
            "release_builder",
            "remoteexec",
            "win",
            "x86",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "chrome/updater:all",
        ],
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (32)",
        short_name = "bld",
    ),
    contact_team_email = "omaha-core@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "win10-updater-tester-dbg",
    description_html = _UPDATER_LINK + " Windows 10 x64 debug tester.",
    parent = "win-updater-builder-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_win",
        ],
        mixins = [
            "win10",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "10",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "win10-32-on-64-updater-tester-dbg",
    description_html = _UPDATER_LINK + " Windows 10 32-on-64 debug tester.",
    parent = "win32-updater-builder-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_win",
        ],
        mixins = [
            "win10",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (32)",
        short_name = "10 (x64)",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "win10-32-on-64-updater-tester-rel",
    description_html = _UPDATER_LINK + " Windows 10 32-on-64 release tester.",
    parent = "win32-updater-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_win",
        ],
        mixins = [
            "win10",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|win (32)",
        short_name = "10 (x64)",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "win10-updater-tester-dbg-uac",
    description_html = _UPDATER_LINK + " Windows 10 x64 debug tester with UAC on.",
    parent = "win-updater-builder-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_win_uac",
        ],
        mixins = [
            "win10-any",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "UAC10",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "win10-updater-tester-rel",
    description_html = _UPDATER_LINK + " Windows 10 x64 release tester.",
    parent = "win-updater-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_win",
        ],
        mixins = [
            "win10",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "10",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "win10-updater-tester-rel-uac",
    description_html = _UPDATER_LINK + " Windows 10 x64 release tester with UAC on.",
    parent = "win-updater-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_win_uac",
        ],
        mixins = [
            "win10-any",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "UAC10",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.builder(
    name = "win11-arm64-updater-tester-dbg",
    description_html = _UPDATER_LINK + " Windows 11 arm64 debug binary tester.",
    parent = "win-arm64-updater-builder-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_win",
        ],
        mixins = [
            "win11-any",
            "arm64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (arm64)",
        short_name = "11",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "win11-arm64-updater-tester-rel",
    description_html = _UPDATER_LINK + " Windows 11 arm64 release tester.",
    parent = "win-arm64-updater-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_win",
        ],
        mixins = [
            "win11-any",
            "arm64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|win (arm64)",
        short_name = "11",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "win11-updater-tester-dbg-uac",
    description_html = _UPDATER_LINK + " Windows 11 x64 debug tester with UAC on.",
    parent = "win-updater-builder-dbg",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_win_uac",
        ],
        mixins = [
            "win11-any",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "UAC11",
    ),
    contact_team_email = "omaha-core@google.com",
)

ci.thin_tester(
    name = "win11-updater-tester-rel",
    description_html = _UPDATER_LINK + " Windows 11 x64 release tester with UAC on.",
    parent = "win-updater-builder-rel",
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    targets = targets.bundle(
        targets = [
            "updater_gtests_win",
        ],
        mixins = [
            "win11-any",
            "x86-64",
        ],
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "11",
    ),
    contact_team_email = "omaha-core@google.com",
)
