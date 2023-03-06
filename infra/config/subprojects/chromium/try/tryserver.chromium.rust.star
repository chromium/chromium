# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.rust builder group."""

load("//lib/builders.star", "os", "reclient")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.rust",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.rust",
)

try_.builder(
    name = "android-rust-arm32-rel",
    mirrors = ["ci/android-rust-arm32-rel"],
)

try_.builder(
    name = "android-rust-arm64-dbg",
    mirrors = ["ci/android-rust-arm64-dbg"],
)

try_.builder(
    name = "android-rust-arm64-rel",
    mirrors = ["ci/android-rust-arm64-rel"],
)

try_.builder(
    name = "linux-rust-x64-rel",
    mirrors = ["ci/linux-rust-x64-rel"],
)

try_.builder(
    name = "linux-rust-x64-dbg",
    mirrors = ["ci/linux-rust-x64-dbg"],
)

try_.builder(
    name = "win-rust-x64-rel",
    mirrors = ["ci/win-rust-x64-rel"],
    os = os.WINDOWS_DEFAULT,
)

try_.builder(
    name = "win-rust-x64-dbg",
    mirrors = ["ci/win-rust-x64-dbg"],
    os = os.WINDOWS_DEFAULT,
)

try_.builder(
    name = "mac-rust-x64-rel",
    mirrors = ["ci/mac-rust-x64-rel"],
    cores = None,
    os = os.MAC_ANY,
)

try_.builder(
    name = "linux-rust-x64-rel-android-toolchain",
    mirrors = ["ci/linux-rust-x64-rel"],
)
