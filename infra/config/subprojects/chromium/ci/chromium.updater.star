# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.updater builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "builders", "cpu", "goma", "os", "reclient")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.updater",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
)

consoles.console_view(
    name = "chromium.updater",
)

# The chromium.updater console includes some entries from official chrome builders.
[branches.console_view_entry(
    builder = "chrome:official/{}".format(name),
    console_view = "chromium.updater",
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
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "bld",
    ),
    cores = None,
    os = os.MAC_ANY,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
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
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "bld",
    ),
    cores = None,
    os = os.MAC_ANY,
    goma_backend = None,
    reclient_jobs = reclient.jobs.DEFAULT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
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
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "bld",
    ),
    cores = None,
    cpu = cpu.ARM64,
    os = os.MAC_ANY,
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
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "bld",
    ),
    cores = None,
    cpu = cpu.ARM64,
    os = os.MAC_ANY,
)

ci.thin_tester(
    name = "mac10.13-updater-tester-dbg",
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
        category = "debug|mac",
        short_name = "10.13",
    ),
    triggered_by = ["mac-updater-builder-dbg"],
)

ci.thin_tester(
    name = "mac10.13-updater-tester-rel",
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
    triggered_by = ["mac-updater-builder-rel"],
)

ci.thin_tester(
    name = "mac10.14-updater-tester-dbg",
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
        category = "debug|mac",
        short_name = "10.14",
    ),
    triggered_by = ["mac-updater-builder-dbg"],
)

ci.thin_tester(
    name = "mac10.14-updater-tester-rel",
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
    triggered_by = ["mac-updater-builder-rel"],
)

ci.thin_tester(
    name = "mac10.15-updater-tester-dbg",
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
        category = "debug|mac",
        short_name = "10.15",
    ),
    triggered_by = ["mac-updater-builder-dbg"],
)

ci.thin_tester(
    name = "mac10.15-updater-tester-rel",
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
    triggered_by = ["mac-updater-builder-rel"],
)

ci.thin_tester(
    name = "mac11.0-updater-tester-dbg",
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
        category = "debug|mac",
        short_name = "11.0",
    ),
    triggered_by = ["mac-updater-builder-dbg"],
)

ci.thin_tester(
    name = "mac11.0-updater-tester-rel",
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
        short_name = "11.0",
    ),
    triggered_by = ["mac-updater-builder-rel"],
)

ci.thin_tester(
    name = "mac-arm64-updater-tester-dbg",
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
        category = "debug|mac",
        short_name = "11.0 arm64",
    ),
    triggered_by = ["mac-updater-builder-arm64-dbg"],
)

ci.thin_tester(
    name = "mac-arm64-updater-tester-rel",
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
        short_name = "11.0 arm64",
    ),
    triggered_by = ["mac-updater-builder-arm64-rel"],
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
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "bld",
    ),
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
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
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (32)",
        short_name = "bld",
    ),
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT * 2,
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    free_space = builders.free_space.high,
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
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "bld",
    ),
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
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
    console_view_entry = consoles.console_view_entry(
        category = "release|win (32)",
        short_name = "bld",
    ),
    os = os.WINDOWS_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
)

ci.thin_tester(
    name = "win7-updater-tester-rel",
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
        short_name = "7",
    ),
    triggered_by = ["win-updater-builder-rel"],
)

ci.thin_tester(
    name = "win7(32)-updater-tester-rel",
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
        category = "release|win (32)",
        short_name = "7",
    ),
    triggered_by = ["win32-updater-builder-rel"],
)

ci.thin_tester(
    name = "win10-updater-tester-dbg",
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
        category = "debug|win (64)",
        short_name = "10",
    ),
    triggered_by = ["win-updater-builder-dbg"],
)

ci.thin_tester(
    name = "win10-32-on-64-updater-tester-dbg",
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
        category = "debug|win (32)",
        short_name = "10 (x64)",
    ),
    triggered_by = ["win32-updater-builder-dbg"],
)

ci.thin_tester(
    name = "win10-updater-tester-dbg-uac",
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
        category = "debug|win (64)",
        short_name = "UAC10",
    ),
    triggered_by = ["win-updater-builder-dbg"],
)

ci.thin_tester(
    name = "win10-updater-tester-rel",
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
    triggered_by = ["win-updater-builder-rel"],
)

ci.thin_tester(
    name = "win11-updater-tester-dbg-uac",
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
        category = "debug|win (64)",
        short_name = "UAC11",
    ),
    triggered_by = ["win-updater-builder-dbg"],
)
