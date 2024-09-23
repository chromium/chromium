# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders used by Tricium for Chromium."""

load("//lib/builders.star", "cpu", "os", "siso")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/try.star", "SOURCELESS_BUILDER_CACHE", "try_")
load("//lib/xcode.star", "xcode")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.tricium",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 2,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    # TODO: b/336209927 - Migrate tricium_clang_tidy_script.py to Siso.
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,

    # Make each bot specify its own OS, since we have a variety of these in this
    # file.
)

consoles.list_view(
    name = "tryserver.chromium.tricium",
)

# This orchestrator differs from traditional orchestrators:
# - we invoke _multiple_ builders which are conceptually compilators
# - these builders exist in various pools with differing platform constraints
# - these builders use standard builder core counts, rather than compilator core
#   counts
# - these builders do not upload build artifacts; they only communicate with
#   this orchestrator via `output` properties.
# Due to these divergences, we roll our own orchestration scheme here.
try_.builder(
    name = "tricium-clang-tidy",
    executable = "recipe:tricium_clang_tidy_orchestrator",
    builderless = False,
    cores = try_.defaults.orchestrator_cores.get(),
    os = os.LINUX_DEFAULT,
    # src checkouts are only required by bots spawned by this builder.
    caches = [SOURCELESS_BUILDER_CACHE],
)

# Clang-tidy builders potentially spawned by the `tricium-clang-tidy`
# orchestrator.
try_.builder(
    name = "android-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "release_try_builder",
            "remoteexec",
            "strip_debug_info",
            "arm",
        ],
    ),
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "fuchsia-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            "fuchsia",
            "x64",
        ],
    ),
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "linux-chromeos-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    gn_args = gn_args.config(
        configs = [
            "chromeos_with_codecs",
            "release_try_builder",
            "remoteexec",
            "x64",
        ],
    ),
    builderless = False,
    os = os.LINUX_DEFAULT,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            "linux",
            "x64",
        ],
    ),
    builderless = False,
    os = os.LINUX_DEFAULT,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            "mac",
            "x64",
        ],
    ),
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    ssd = True,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
    # TODO(gbiv): Determine why this needs a system xcode and things like `Mac
    # Builder` don't.
    xcode = xcode.x13main,
)

try_.builder(
    name = "win10-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    gn_args = gn_args.config(
        configs = [
            "release_try_builder",
            "remoteexec",
            "win",
            "x64",
        ],
    ),
    os = os.WINDOWS_DEFAULT,
)
