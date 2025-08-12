# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.mac builder group."""

load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//try.star", "try_")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.cft",
    pool = try_constants.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.cft",
)

try_.builder(
    name = "linux-rel-cft",
    mirrors = [
        "ci/linux-rel-cft",
    ],
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            "no_symbols",
            "devtools_do_typecheck",
            "chrome_for_testing",
            "linux",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "mac-rel-cft",
    mirrors = [
        "ci/mac-rel-cft",
    ],
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            "no_symbols",
            "chrome_for_testing",
            "mac",
            "x64",
        ],
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
)

try_.builder(
    name = "win-rel-cft",
    mirrors = [
        "ci/win-rel-cft",
    ],
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            # TODO(crbug.com/40099061) Delete this once coverage mode is enabled
            # on the standard Windows trybot and the dedicated coverage trybot
            # is no longer needed.
            "no_resource_allowlisting",
            "chrome_for_testing",
            "win",
            "x64",
        ],
    ),
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
)
