# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.accessibility builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.chromium.accessibility",
    cores = 8,
    compilator_cores = 16,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    compilator_goma_jobs = goma.jobs.J150,
    os = os.LINUX_DEFAULT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.accessibility",
)

try_.builder(
    name = "linux-blink-web-tests-force-accessibility-rel",
    mirrors = ["ci/linux-blink-web-tests-force-accessibility-rel"],
)
