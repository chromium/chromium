# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.accessibility builder group."""

load("//lib/builders.star", "os", "siso")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.accessibility",
    pool = try_.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 16,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_enabled = True,
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
