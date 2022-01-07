# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.infra builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.infra",
    cores = 8,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.infra",
)

try_.builder(
    name = "linux-bootstrap",
    mirrors = [
        "ci/linux-bootstrap",
        "ci/linux-bootstrap-tests",
    ],
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
)

try_.builder(
    name = "win-bootstrap",
    builderless = True,
    os = os.WINDOWS_10,
)
