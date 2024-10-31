# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.fuzz builder group."""

load("//lib/builders.star", "cpu", "os", "siso")
load("//lib/consoles.star", "consoles")
load("//lib/try.star", "try_")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.fuzz",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    os = os.LINUX_DEFAULT,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.fuzz",
)

try_.builder(
    name = "linux-asan-dbg",
    mirrors = ["ci/ASAN Debug"],
    gn_args = "ci/ASAN Debug",
)

try_.builder(
    name = "linux-asan-rel",
    mirrors = ["ci/ASAN Release"],
    gn_args = "ci/ASAN Release",
)

try_.builder(
    name = "linux-asan-media-rel",
    mirrors = ["ci/ASAN Release Media"],
    gn_args = "ci/ASAN Release Media",
)

try_.builder(
    name = "linux-asan-v8-arm-dbg",
    mirrors = ["ci/ASan Debug (32-bit x86 with V8-ARM)"],
    gn_args = "ci/ASan Debug (32-bit x86 with V8-ARM)",
)

try_.builder(
    name = "linux-asan-v8-arm-rel",
    mirrors = ["ci/ASan Release (32-bit x86 with V8-ARM)"],
    gn_args = "ci/ASan Release (32-bit x86 with V8-ARM)",
)

try_.builder(
    name = "linux-asan-media-v8-arm-rel",
    mirrors = ["ci/ASan Release Media (32-bit x86 with V8-ARM)"],
    gn_args = "ci/ASan Release Media (32-bit x86 with V8-ARM)",
)

try_.builder(
    name = "linux-chromeos-asan-rel",
    mirrors = ["ci/ChromiumOS ASAN Release"],
    gn_args = "ci/ChromiumOS ASAN Release",
)

try_.builder(
    name = "linux-msan-chained-origins-rel",
    mirrors = ["ci/MSAN Release (chained origins)"],
    gn_args = "ci/MSAN Release (chained origins)",
)

try_.builder(
    name = "linux-msan-no-origins-rel",
    mirrors = ["ci/MSAN Release (no origins)"],
    gn_args = "ci/MSAN Release (no origins)",
)

try_.builder(
    name = "linux-tsan-dbg",
    mirrors = ["ci/TSAN Debug"],
    gn_args = "ci/TSAN Debug",
)

try_.builder(
    name = "linux-tsan-rel",
    mirrors = ["ci/TSAN Release"],
    gn_args = "ci/TSAN Release",
)

try_.builder(
    name = "linux-ubsan-rel",
    mirrors = ["ci/UBSan Release"],
    gn_args = "ci/UBSan Release",
)

try_.builder(
    name = "linux-ubsan-vptr-rel",
    mirrors = ["ci/UBSan vptr Release"],
    gn_args = "ci/UBSan vptr Release",
)

try_.builder(
    name = "mac-asan-rel",
    mirrors = ["ci/Mac ASAN Release"],
    gn_args = "ci/Mac ASAN Release",
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
)

try_.builder(
    name = "mac-asan-media-rel",
    mirrors = ["ci/Mac ASAN Release Media"],
    gn_args = "ci/Mac ASAN Release Media",
    cores = None,
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "win-asan-rel",
    mirrors = ["ci/Win ASan Release"],
    gn_args = "ci/Win ASan Release",
    os = os.WINDOWS_DEFAULT,
)

try_.builder(
    name = "win-asan-media-rel",
    mirrors = ["ci/Win ASan Release Media"],
    gn_args = "ci/Win ASan Release Media",
    os = os.WINDOWS_DEFAULT,
)
