# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.rust builder group."""

load("//lib/builders.star", "goma", "os", "reclient")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.rust",
    pool = try_.DEFAULT_POOL,
    builderless = False,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.rust",
)

try_.builder(
    name = "android-rust-arm-dbg",
    mirrors = ["ci/android-rust-arm-dbg"],
    goma_backend = None,
)

try_.builder(
    name = "android-rust-arm-rel",
    mirrors = ["ci/android-rust-arm-rel"],
    goma_backend = None,
)

try_.builder(
    name = "linux-rust-x64-rel",
    mirrors = ["ci/linux-rust-x64-rel"],
    goma_backend = None,
)

try_.builder(
    name = "linux-rust-x64-rel-android-toolchain",
    mirrors = ["ci/linux-rust-x64-rel"],
)

try_.builder(
    name = "linux-rust-x64-dbg",
    mirrors = ["ci/linux-rust-x64-dbg"],
    goma_backend = None,
)
