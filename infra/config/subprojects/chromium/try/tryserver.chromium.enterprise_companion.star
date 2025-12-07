# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.enterprise_companion builder group."""

load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//html.star", "linkify")
load("@chromium-luci//try.star", "try_")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.enterprise_companion",
    pool = try_constants.DEFAULT_POOL,
    builderless = True,
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.enterprise_companion",
)

def enterprise_companion_linux_builder(*, name, **kwargs):
    kwargs.setdefault("os", os.LINUX_DEFAULT)
    kwargs.setdefault("siso_project", siso.project.DEFAULT_UNTRUSTED)
    kwargs.setdefault("siso_remote_jobs", siso.remote_jobs.LOW_JOBS_FOR_CQ)
    return try_.builder(
        name = name,
        contact_team_email = "omaha-client-dev@google.com",
        **kwargs
    )

def enterprise_companion_mac_builder(*, name, **kwargs):
    kwargs.setdefault("os", os.MAC_DEFAULT)
    return try_.builder(
        name = name,
        contact_team_email = "omaha-client-dev@google.com",
        **kwargs
    )

def enterprise_companion_windows_builder(*, name, **kwargs):
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.WINDOWS_DEFAULT)
    return try_.builder(
        name = name,
        contact_team_email = "omaha-client-dev@google.com",
        **kwargs
    )

enterprise_companion_linux_builder(
    name = "linux-enterprise-companion-try-builder-dbg",
    description_html = "Compiles and runs " + linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Linux Debug tests.",
    mirrors = [
        "ci/linux-enterprise-companion-builder-dbg",
        "ci/linux-enterprise-companion-tester-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-enterprise-companion-builder-dbg",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/enterprise_companion/.+",
        ],
    ),
)

enterprise_companion_linux_builder(
    name = "linux-enterprise-companion-try-builder-rel",
    description_html = "Compiles and runs " + linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Linux Release tests.",
    mirrors = [
        "ci/linux-enterprise-companion-builder-rel",
        "ci/linux-enterprise-companion-tester-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-enterprise-companion-builder-rel",
            "release_try_builder",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/enterprise_companion/.+",
        ],
    ),
)

enterprise_companion_mac_builder(
    name = "mac-enterprise-companion-try-builder-dbg",
    description_html = "Compiles and runs " + linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Mac Debug tests.",
    mirrors = [
        "ci/mac-enterprise-companion-builder-arm64-dbg",
        "ci/mac13-arm64-enterprise-companion-tester-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/mac-enterprise-companion-builder-arm64-dbg",
        ],
    ),
    cpu = cpu.ARM64,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/enterprise_companion/.+",
        ],
    ),
)

enterprise_companion_mac_builder(
    name = "mac-enterprise-companion-try-builder-rel",
    description_html = "Compiles and runs " + linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Mac Release tests.",
    mirrors = [
        "ci/mac-enterprise-companion-builder-rel",
        "ci/mac13-x64-enterprise-companion-tester-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/mac-enterprise-companion-builder-rel",
            "release_try_builder",
        ],
    ),
    cpu = cpu.ARM64,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/enterprise_companion/.+",
        ],
    ),
)

enterprise_companion_windows_builder(
    name = "win-enterprise-companion-try-builder-dbg",
    description_html = "Compiles and runs " + linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows Debug tests.",
    mirrors = [
        "ci/win-enterprise-companion-builder-dbg",
        "ci/win10-enterprise-companion-tester-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win-enterprise-companion-builder-dbg",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/enterprise_companion/.+",
        ],
    ),
)

enterprise_companion_windows_builder(
    name = "win-enterprise-companion-try-builder-rel",
    description_html = "Compiles and runs " + linkify("https://source.chromium.org/chromium/chromium/src/+/main:chrome/enterprise_companion/README.md", "Chrome Enterprise Companion App") + " Windows Release tests.",
    mirrors = [
        "ci/win-enterprise-companion-builder-rel",
        "ci/win10-enterprise-companion-tester-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/win-enterprise-companion-builder-rel",
            "release_try_builder",
        ],
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/enterprise_companion/.+",
        ],
    ),
)
