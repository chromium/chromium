# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.accessibility builder group."""

load("@chromium-luci//builders.star", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//try.star", "try_")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.accessibility",
    pool = try_constants.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 16,
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.accessibility",
)

try_.builder(
    name = "fuchsia-x64-accessibility-rel",
    mirrors = ["ci/fuchsia-x64-accessibility-rel"],
    gn_args = gn_args.config(
        configs = [
            "ci/fuchsia-x64-accessibility-rel",
        ],
    ),
    tryjob = try_.job(
        location_filters = [
            "third_party/blink/renderer/modules/accessibility/.+",
            "content/renderer/accessibility/.+",
            "content/browser/accessibility/.+",
            "ui/accessibility/.+",
        ],
    ),
)

try_.builder(
    name = "linux-blink-web-tests-force-accessibility-rel",
    mirrors = ["ci/linux-blink-web-tests-force-accessibility-rel"],
    gn_args = gn_args.config(
        configs = [
            "ci/linux-blink-web-tests-force-accessibility-rel",
        ],
    ),
    check_for_flakiness = False,
    check_for_flakiness_with_resultdb = False,
    tryjob = try_.job(
        location_filters = [
            "third_party/blink/renderer/modules/accessibility/.+",
            "content/renderer/accessibility/.+",
            "content/browser/accessibility/.+",
            "ui/accessibility/.+",
            "ui/views/accessibility/.+",
        ],
    ),
)
