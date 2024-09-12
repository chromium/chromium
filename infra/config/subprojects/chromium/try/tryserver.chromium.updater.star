# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.updater builder group."""

load("//lib/builders.star", "cpu", "os", "siso")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.updater",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.updater",
)

def updater_linux_builder(*, name, **kwargs):
    kwargs.setdefault("os", os.LINUX_DEFAULT)
    kwargs.setdefault("siso_project", siso.project.DEFAULT_UNTRUSTED)
    kwargs.setdefault("siso_remote_jobs", siso.remote_jobs.LOW_JOBS_FOR_CQ)
    return try_.builder(name = name, **kwargs)

def updater_mac_builder(*, name, **kwargs):
    kwargs.setdefault("os", os.MAC_ANY)
    return try_.builder(name = name, **kwargs)

def updater_windows_builder(*, name, **kwargs):
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.WINDOWS_DEFAULT)
    return try_.builder(name = name, **kwargs)

updater_linux_builder(
    name = "linux-updater-try-builder-dbg",
    mirrors = [
        "ci/linux-updater-builder-dbg",
        "ci/linux-updater-tester-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-updater-builder-dbg",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/updater/.+",
        ],
    ),
)

updater_linux_builder(
    name = "linux-updater-try-builder-rel",
    mirrors = [
        "ci/linux-updater-builder-rel",
        "ci/linux-updater-tester-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-updater-builder-rel",
            "release_try_builder",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/updater/.+",
        ],
    ),
)

updater_mac_builder(
    name = "mac-updater-try-builder-dbg",
    mirrors = [
        "ci/mac-updater-builder-dbg",
        "ci/mac11-x64-updater-tester-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/mac-updater-builder-dbg",
        ],
    ),
    cores = None,
    cpu = cpu.ARM64,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/updater/.+",
        ],
    ),
)

updater_mac_builder(
    name = "mac-updater-try-builder-rel",
    mirrors = [
        "ci/mac-updater-builder-rel",
        "ci/mac11-x64-updater-tester-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/mac-updater-builder-rel",
            "release_try_builder",
        ],
    ),
    cpu = cpu.ARM64,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/updater/.+",
        ],
    ),
)

updater_windows_builder(
    name = "win-updater-try-builder-dbg",
    mirrors = [
        "ci/win-updater-builder-dbg",
        "ci/win10-updater-tester-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win-updater-builder-dbg",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/updater/.+",
        ],
    ),
)

updater_windows_builder(
    name = "win-updater-try-builder-rel",
    mirrors = [
        "ci/win-updater-builder-rel",
        "ci/win10-updater-tester-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win-updater-builder-rel",
            "release_try_builder",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/updater/.+",
        ],
    ),
)
