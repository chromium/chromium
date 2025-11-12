# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.crossbench builder group."""

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//try.star", "try_")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.crossbench",
    pool = try_constants.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.crossbench",
    branch_selector = branches.selector.MAIN,
)

try_.builder(
    name = "linux-crossbench",
    description_html = "Run Crossbench Smoke tests on Linux.",
    mirrors = ["ci/linux-crossbench"],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-crossbench",
        ],
    ),
    contact_team_email = "crossbench-infra-vteam@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            # Inclusion filters.
            cq.location_filter(path_regexp = "third_party/crossbench/.+"),
            cq.location_filter(path_regexp = "third_party/speedometer/.+"),
        ],
    ),
)
