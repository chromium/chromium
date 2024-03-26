# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.mac builder group."""

load("//lib/builders.star", "os", "reclient", "siso")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.cft",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_configs = ["builder"],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
)

consoles.list_view(
    name = "tryserver.chromium.cft",
)

try_.builder(
    name = "linux-arm64-rel-cft",
    mirrors = [
        "ci/linux-arm64-rel-cft",
    ],
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "reclient",
            "no_symbols",
            "chrome_for_testing",
            "arm64",
        ],
    ),
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "linux-rel-cft",
    mirrors = [
        "ci/linux-rel-cft",
    ],
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "reclient",
            "no_symbols",
            "devtools_do_typecheck",
            "chrome_for_testing",
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
            "reclient",
            "no_symbols",
            "chrome_for_testing",
        ],
    ),
    cores = None,
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "win-rel-cft",
    mirrors = [
        "ci/win-rel-cft",
    ],
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "reclient",
            # TODO(crbug.com/1004523) Delete this once coverage mode is enabled
            # on the standard Windows trybot and the dedicated coverage trybot
            # is no longer needed.
            "no_resource_allowlisting",
            "chrome_for_testing",
        ],
    ),
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
)
