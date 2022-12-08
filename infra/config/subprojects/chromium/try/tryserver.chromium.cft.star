# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.mac builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.chromium.cft",
    builderless = True,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    ssd = True,
)

consoles.list_view(
    name = "tryserver.chromium.cft",
)

try_.builder(
    name = "mac-rel-cft",
    mirrors = [
        "ci/mac-rel-cft",
    ],
    cores = None,
    os = os.MAC_DEFAULT,
)
