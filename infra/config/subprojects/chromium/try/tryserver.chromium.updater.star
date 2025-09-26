# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.updater builder group."""

load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify")
load("@chromium-luci//try.star", "try_")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.updater",
    pool = try_constants.DEFAULT_POOL,
    builderless = True,
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.updater",
)

_UPDATER_LINK = linkify("https://chromium.googlesource.com/chromium/src/+/main/docs/updater/design_doc.md", "Chromium updater")
_LOCATION_FILTER = ["chrome/updater/.+", "chrome/enterprise_companion/.+"]

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
    description_html = _UPDATER_LINK + " Linux x64 debug builder.",
    mirrors = [
        "ci/linux-updater-builder-dbg",
        "ci/linux-updater-tester-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-updater-builder-dbg",
        ],
    ),
    contact_team_email = "omaha@google.com",
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = _LOCATION_FILTER,
    ),
)

updater_linux_builder(
    name = "linux-updater-try-builder-rel",
    description_html = _UPDATER_LINK + " Linux x64 release builder.",
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
    contact_team_email = "omaha@google.com",
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = _LOCATION_FILTER,
    ),
)

updater_mac_builder(
    name = "mac-updater-try-builder-dbg",
    description_html = _UPDATER_LINK + " macOS 11 x64 debug builder.",
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
    contact_team_email = "omaha@google.com",
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = _LOCATION_FILTER,
    ),
)

updater_mac_builder(
    name = "mac-updater-try-builder-rel",
    description_html = _UPDATER_LINK + " macOS 11 x64 release builder.",
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
    contact_team_email = "omaha@google.com",
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = _LOCATION_FILTER,
    ),
)

updater_windows_builder(
    name = "win-arm64-updater-rel",
    description_html = _UPDATER_LINK + " Windows 11 arm64 release builder.",
    mirrors = [
        "ci/win-arm64-updater-builder-rel",
        "ci/win11-arm64-updater-tester-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win-arm64-updater-builder-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "omaha@google.com",
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = _LOCATION_FILTER,
    ),
)

updater_windows_builder(
    name = "win-updater-try-builder-dbg",
    description_html = _UPDATER_LINK + " Windows 10 x64 debug builder.",
    mirrors = [
        "ci/win-updater-builder-dbg",
        "ci/win10-updater-tester-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win-updater-builder-dbg",
        ],
    ),
    contact_team_email = "omaha@google.com",
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = _LOCATION_FILTER,
    ),
)

updater_windows_builder(
    name = "win-updater-try-builder-rel",
    description_html = _UPDATER_LINK + " Windows 10 x64 release builder.",
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
    contact_team_email = "omaha@google.com",
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = _LOCATION_FILTER,
    ),
)
