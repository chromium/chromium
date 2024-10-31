# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "cpu", "os", "siso")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
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
    gn_args = gn_args.config(
        configs = [
            "ci/android-official",
            # TODO(crbug.com/41490911): Restore DCHECKs when the build is fixed.
            #"dcheck_always_on",
        ],
    ),
    builderless = False,
    contact_team_email = "clank-engprod@google.com",
)

try_.builder(
    name = "linux-official",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/linux-official",
    ],
    gn_args = gn_args.config(
        configs = ["ci/linux-official", "try_builder"],
    ),
    ssd = True,
)

try_.builder(
    name = "mac-official",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/mac-official",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/mac-official",
            "minimal_symbols",
            "dcheck_always_on",
        ],
    ),
    builderless = False,
    cores = None,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    # TODO(crbug.com/40208487) builds with PGO change take long time.
    # Keep in sync with mac-official in ci/chromium.star.
    execution_timeout = 15 * time.hour,
)

try_.builder(
    name = "win-official",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/win-official",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win-official",
            "dcheck_always_on",
        ],
    ),
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "win32-official",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/win32-official",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win32-official",
            "minimal_symbols",
            "dcheck_always_on",
        ],
    ),
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
)
