# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "cpu", "os", "reclient")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
        branches.selector.DESKTOP_BRANCHES,
        branches.selector.FUCHSIA_BRANCHES,
    ],
)

try_.builder(
    name = "android-official",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-official",
    ],
    ssd = True,
)

try_.builder(
    name = "fuchsia-official",
    branch_selector = branches.selector.FUCHSIA_BRANCHES,
    mirrors = [
        "ci/fuchsia-official",
    ],
    ssd = True,
)

try_.builder(
    name = "linux-official",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/linux-official",
    ],
    ssd = True,
)

try_.builder(
    name = "mac-official",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/mac-official",
    ],
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    # TODO(crbug.com/1279290) builds with PGO change take long time.
    # Keep in sync with mac-official in ci/chromium.star.
    execution_timeout = 15 * time.hour,
)

try_.builder(
    name = "win-official",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/win-official",
    ],
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "win32-official",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/win32-official",
    ],
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
)
