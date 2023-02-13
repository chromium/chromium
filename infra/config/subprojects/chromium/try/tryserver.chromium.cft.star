# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.mac builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.cft",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    check_for_flakiness = True,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.cft",
)

try_.builder(
    name = "linux-rel-cft",
    mirrors = [
        "ci/linux-rel-cft",
    ],
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "mac-rel-cft",
    mirrors = [
        "ci/mac-rel-cft",
    ],
    cores = None,
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "win-rel-cft",
    mirrors = [
        "ci/win-rel-cft",
    ],
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
)
