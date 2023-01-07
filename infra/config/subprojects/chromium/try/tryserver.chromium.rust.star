# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.rust builder group."""

load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.chromium.rust",
    builderless = False,
    cores = 8,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    os = os.LINUX_DEFAULT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.rust",
)

try_.builder(
    name = "android-rust-arm-dbg",
)

try_.builder(
    name = "android-rust-arm-rel",
)

try_.builder(
    name = "linux-rust-x64-rel",
)

try_.builder(
    name = "linux-rust-x64-rel-android-toolchain",
)

try_.builder(
    name = "linux-rust-x64-dbg",
)
