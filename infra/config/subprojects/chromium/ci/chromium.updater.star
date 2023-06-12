# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.updater builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "os", "reclient")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.updater",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
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
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
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
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
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
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "bld-asan",
    ),
)

ci.thin_tester(
    name = "mac10.13-updater-tester-dbg",
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
        short_name = "10.13",
    ),
)

ci.thin_tester(
    name = "mac10.13-updater-tester-rel",
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
        short_name = "10.13",
    ),
)

ci.thin_tester(
    name = "mac10.14-updater-tester-dbg",
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
        short_name = "10.14",
    ),
)

ci.thin_tester(
    name = "mac10.14-updater-tester-rel",
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
        short_name = "10.14",
    ),
)

ci.thin_tester(
    name = "mac10.15-updater-tester-dbg",
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
        short_name = "10.15",
    ),
)

ci.thin_tester(
    name = "mac10.15-updater-tester-rel",
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
        short_name = "10.15",
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
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "bld",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
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
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    free_space = builders.free_space.high,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (32)",
        short_name = "bld",
    ),
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT * 2,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
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
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "bld",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
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
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (32)",
        short_name = "bld",
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
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
