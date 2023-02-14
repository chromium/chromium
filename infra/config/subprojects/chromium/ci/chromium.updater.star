# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.updater builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "goma", "os", "reclient")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.updater",
    pool = ci.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
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
    name = "mac-updater-builder-dbg",
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
    builderless = True,
    cores = None,
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "bld",
    ),
)

ci.thin_tester(
    name = "mac10.11-updater-tester-dbg",
    triggered_by = ["mac-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "10.11",
    ),
)

ci.thin_tester(
    name = "mac10.11-updater-tester-rel",
    triggered_by = ["mac-updater-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "10.11",
    ),
)

ci.thin_tester(
    name = "mac10.12-updater-tester-dbg",
    triggered_by = ["mac-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "10.12",
    ),
)

ci.thin_tester(
    name = "mac10.12-updater-tester-rel",
    triggered_by = ["mac-updater-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "10.12",
    ),
)

ci.thin_tester(
    name = "mac10.13-updater-tester-dbg",
    triggered_by = ["mac-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "10.13",
    ),
)

ci.thin_tester(
    name = "mac10.13-updater-tester-rel",
    triggered_by = ["mac-updater-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "10.13",
    ),
)

ci.thin_tester(
    name = "mac10.14-updater-tester-dbg",
    triggered_by = ["mac-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "10.14",
    ),
)

ci.thin_tester(
    name = "mac10.14-updater-tester-rel",
    triggered_by = ["mac-updater-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "10.14",
    ),
)

ci.thin_tester(
    name = "mac10.15-updater-tester-dbg",
    triggered_by = ["mac-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "10.15",
    ),
)

ci.thin_tester(
    name = "mac10.15-updater-tester-rel",
    triggered_by = ["mac-updater-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "10.15",
    ),
)

ci.thin_tester(
    name = "mac11.0-updater-tester-dbg",
    triggered_by = ["mac-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "11.0",
    ),
)

ci.thin_tester(
    name = "mac11.0-updater-tester-rel",
    triggered_by = ["mac-updater-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "11.0",
    ),
)

ci.thin_tester(
    name = "mac-arm64-updater-tester-dbg",
    triggered_by = ["mac-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|mac",
        short_name = "11.0 arm64",
    ),
)

ci.thin_tester(
    name = "mac-arm64-updater-tester-rel",
    triggered_by = ["mac-updater-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "release|mac",
        short_name = "11.0 arm64",
    ),
)

ci.builder(
    name = "win-updater-builder-dbg",
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "bld",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win32-updater-builder-dbg",
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (32)",
        short_name = "bld",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win-updater-builder-rel",
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "bld",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.builder(
    name = "win32-updater-builder-rel",
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|win (32)",
        short_name = "bld",
    ),
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CI,
)

ci.thin_tester(
    name = "win7-updater-tester-dbg",
    triggered_by = ["win-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "7",
    ),
)

ci.thin_tester(
    name = "win7-updater-tester-dbg-uac",
    triggered_by = ["win-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "UAC7",
    ),
)

ci.thin_tester(
    name = "win7(32)-updater-tester-dbg",
    triggered_by = ["win32-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (32)",
        short_name = "7",
    ),
)

ci.thin_tester(
    name = "win7-updater-tester-rel",
    triggered_by = ["win-updater-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "7",
    ),
)

ci.thin_tester(
    name = "win7(32)-updater-tester-rel",
    triggered_by = ["win32-updater-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "release|win (32)",
        short_name = "7",
    ),
)

ci.thin_tester(
    name = "win10-updater-tester-dbg",
    triggered_by = ["win-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "10",
    ),
)

ci.thin_tester(
    name = "win10-updater-tester-dbg-uac",
    triggered_by = ["win-updater-builder-dbg"],
    console_view_entry = consoles.console_view_entry(
        category = "debug|win (64)",
        short_name = "UAC10",
    ),
)

ci.thin_tester(
    name = "win10-updater-tester-rel",
    triggered_by = ["win-updater-builder-rel"],
    console_view_entry = consoles.console_view_entry(
        category = "release|win (64)",
        short_name = "10",
    ),
)
