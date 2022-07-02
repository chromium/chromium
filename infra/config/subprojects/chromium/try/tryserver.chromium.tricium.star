# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders used by Tricium for Chromium."""

load("//lib/builders.star", "goma", "os", "xcode")
load("//lib/consoles.star", "consoles")
load("//lib/try.star", "SOURCELESS_BUILDER_CACHES", "try_")

try_.defaults.set(
    builder_group = "tryserver.chromium.tricium",
    builderless = True,
    cores = 8,
    orchestrator_cores = 2,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    goma_jobs = goma.jobs.J150,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,

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
    # src checkouts are only required by bots spawned by this builder.
    caches = SOURCELESS_BUILDER_CACHES,
    cores = try_.defaults.orchestrator_cores.get(),
    os = os.LINUX_DEFAULT,
    goma_backend = None,
)

# Clang-tidy builders potentially spawned by the `tricium-clang-tidy`
# orchestrator.
try_.builder(
    name = "android-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "fuchsia-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "ios-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    builderless = False,
    cores = None,
    os = os.MAC_DEFAULT,
    xcode = xcode.x13main,
)

try_.builder(
    name = "linux-chromeos-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "linux-clang-tidy-dbg",
    executable = "recipe:tricium_clang_tidy_wrapper",
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "linux-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "linux-lacros-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    os = os.LINUX_DEFAULT,
)

try_.builder(
    name = "mac-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    os = os.MAC_DEFAULT,
    cores = None,
    ssd = True,
    # TODO(gbiv): Determine why this needs a system xcode and things like `Mac
    # Builder` don't.
    xcode = xcode.x13main,
)

try_.builder(
    name = "win10-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    os = os.WINDOWS_DEFAULT,
)
