# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.updater builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "os", "siso")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/builder_health_indicators.star", "health_spec")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.updater",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
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

ci.builder(
    name = "linux-updater-builder-dbg",
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
)

ci.builder(
    name = "linux-updater-builder-rel",
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
)

ci.thin_tester(
    name = "linux-updater-tester-dbg",
    triggered_by = ["linux-updater-builder-dbg"],
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|linux",
        short_name = "test",
    ),
)

ci.thin_tester(
    name = "linux-updater-tester-rel",
    triggered_by = ["linux-updater-builder-rel"],
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
    console_view_entry = consoles.console_view_entry(
        category = "release|linux",
        short_name = "test",
    ),
)

ci.builder(
    name = "mac-updater-builder-dbg",
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
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "bld",
    ),
)

ci.builder(
    name = "mac-updater-builder-rel",
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
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "bld",
    ),
)

ci.builder(
    name = "mac-updater-builder-arm64-dbg",
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
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "bld",
    ),
)

ci.builder(
    name = "mac-updater-builder-arm64-rel",
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
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "bld",
    ),
)

ci.builder(
    name = "mac-updater-builder-asan-dbg",
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
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "bld-asan",
    ),
)

ci.thin_tester(
    name = "mac11-arm64-updater-tester-dbg",
    triggered_by = ["mac-updater-builder-arm64-dbg"],
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "11 arm64",
    ),
)

ci.thin_tester(
    name = "mac11-arm64-updater-tester-rel",
    triggered_by = ["mac-updater-builder-arm64-rel"],
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
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "11 arm64",
    ),
)

ci.thin_tester(
    name = "mac11-x64-updater-tester-dbg",
    triggered_by = ["mac-updater-builder-dbg"],
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "11",
    ),
)

ci.thin_tester(
    name = "mac11-x64-updater-tester-rel",
    triggered_by = ["mac-updater-builder-rel"],
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
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "11",
    ),
)

ci.thin_tester(
    name = "mac12-arm64-updater-tester-rel",
    triggered_by = ["mac-updater-builder-arm64-rel"],
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
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "12 arm64",
    ),
)

ci.thin_tester(
    name = "mac12-x64-updater-tester-asan-dbg",
    triggered_by = ["mac-updater-builder-asan-dbg"],
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "12 asan",
    ),
)

ci.thin_tester(
    name = "mac13-arm64-updater-tester-dbg",
    triggered_by = ["mac-updater-builder-arm64-dbg"],
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "13 arm64",
    ),
)

ci.thin_tester(
    name = "mac13-x64-updater-tester-rel",
    triggered_by = ["mac-updater-builder-rel"],
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
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "13",
    ),
)

ci.builder(
    name = "win-updater-builder-dbg",
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
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "bld",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win32-updater-builder-dbg",
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
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (32)",
        short_name = "bld",
    ),
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT * 2,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-updater-builder-rel",
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
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "bld",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win32-updater-builder-rel",
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
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (32)",
        short_name = "bld",
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "win10-updater-tester-dbg",
    triggered_by = ["win-updater-builder-dbg"],
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "10",
    ),
)

ci.thin_tester(
    name = "win10-32-on-64-updater-tester-dbg",
    triggered_by = ["win32-updater-builder-dbg"],
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (32)",
        short_name = "10 (x64)",
    ),
)

ci.thin_tester(
    name = "win10-32-on-64-updater-tester-rel",
    triggered_by = ["win32-updater-builder-rel"],
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
    console_view_entry = consoles.console_view_entry(
        category = "release|win (32)",
        short_name = "10 (x64)",
    ),
)

ci.thin_tester(
    name = "win10-updater-tester-dbg-uac",
    triggered_by = ["win-updater-builder-dbg"],
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "UAC10",
    ),
)

ci.thin_tester(
    name = "win10-updater-tester-rel",
    triggered_by = ["win-updater-builder-rel"],
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
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "10",
    ),
)

ci.thin_tester(
    name = "win10-updater-tester-rel-uac",
    triggered_by = ["win-updater-builder-rel"],
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
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "UAC10",
    ),
)

ci.thin_tester(
    name = "win11-updater-tester-dbg-uac",
    triggered_by = ["win-updater-builder-dbg"],
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
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "UAC11",
    ),
)

ci.thin_tester(
    name = "win11-updater-tester-rel",
    triggered_by = ["win-updater-builder-rel"],
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
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "11",
    ),
)
