# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders used by Tricium for Chromium."""

load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//try.star", "SOURCELESS_BUILDER_CACHE", "try_")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")
load("//lib/xcode.star", "xcode")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.tricium",
    pool = try_constants.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    orchestrator_cores = 2,
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
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
    # We do not have sufficient capacity for tricium-clang-tidy presently, so
    # this results in expiration and causes InfraFailure alerts that troopers
    # have no sustainable mitigation path for
    alerts_enabled = False,
    # src checkouts are only required by bots spawned by this builder.
    caches = [SOURCELESS_BUILDER_CACHE],
    tryjob = try_.job(
        custom_cq_run_modes = [cq.MODE_NEW_PATCHSET_RUN],
        disable_reuse = True,
        experiment_percentage = 100,
        location_filters = [
            cq.location_filter(path_regexp = r".*\.(c|cc|cpp|h)"),
        ],
    ),
)

# Clang-tidy builders potentially spawned by the `tricium-clang-tidy`
# orchestrator.
try_.builder(
    name = "android-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
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
    xcode = xcode.xcode_default,
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
