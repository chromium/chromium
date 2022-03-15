# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.win builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builders.star", "goma", "os", "sheriff_rotations")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    builder_group = "chromium.win",
    cores = 8,
    executable = ci.DEFAULT_EXECUTABLE,
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    main_console_view = "main",
    os = os.WINDOWS_DEFAULT,
    pool = ci.DEFAULT_POOL,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,
)

consoles.console_view(
    name = "chromium.win",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    ordering = {
        None: ["release", "debug"],
        "debug|builder": consoles.ordering(short_names = ["64", "32"]),
        "debug|tester": consoles.ordering(short_names = ["7", "10"]),
    },
)

ci.builder(
    name = "WebKit Win10",
    console_view_entry = consoles.console_view_entry(
        category = "misc",
        short_name = "wbk",
    ),
    triggered_by = ["Win Builder"],
)

ci.builder(
    name = "Win Builder",
    console_view_entry = consoles.console_view_entry(
        category = "release|builder",
        short_name = "32",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.builder(
    name = "Win x64 Builder (dbg)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "64",
    ),
    cores = 32,
    os = os.WINDOWS_ANY,
)

ci.builder(
    name = "Win10 Tests x64 (dbg)",
    console_view_entry = consoles.console_view_entry(
        category = "debug|tester",
        short_name = "10",
    ),
    triggered_by = ["Win x64 Builder (dbg)"],
    # Too flaky. See crbug.com/876224 for more details.
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
)

ci.thin_tester(
    name = "Win7 (32) Tests",
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "32",
    ),
    triggered_by = ["Win Builder"],
)

ci.builder(
    name = "Win7 Tests (1)",
    builderless = True,
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "32",
    ),
    os = os.WINDOWS_10,
    triggered_by = ["Win Builder"],
)

ci.builder(
    name = "Win7 Tests (dbg)(1)",
    builderless = True,
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "debug|tester",
        short_name = "7",
    ),
    cq_mirrors_console_view = "mirrors",
    os = os.WINDOWS_10,
    triggered_by = ["ci/Win Builder (dbg)"],
)

ci.builder(
    name = "Win 7 Tests x64 (1)",
    builderless = True,
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "64",
    ),
    cq_mirrors_console_view = "mirrors",
    os = os.WINDOWS_10,
    triggered_by = ["ci/Win x64 Builder"],
)

ci.builder(
    name = "Win Builder (dbg)",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "debug|builder",
        short_name = "32",
    ),
    cores = 32,
    cq_mirrors_console_view = "mirrors",
    os = os.WINDOWS_ANY,
)

ci.builder(
    name = "Win x64 Builder",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release|builder",
        short_name = "64",
    ),
    cores = 32,
    cq_mirrors_console_view = "mirrors",
    os = os.WINDOWS_ANY,
)

ci.builder(
    name = "Win10 Tests x64",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    console_view_entry = consoles.console_view_entry(
        category = "release|tester",
        short_name = "w10",
    ),
    cq_mirrors_console_view = "mirrors",
    triggered_by = ["ci/Win x64 Builder"],
)

ci.builder(
    name = "Windows deterministic",
    console_view_entry = consoles.console_view_entry(
        category = "misc",
        short_name = "det",
    ),
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 12 * time.hour,
    goma_jobs = goma.jobs.J150,
)
