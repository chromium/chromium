# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "goma", "os", "reclient")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.chromium",
    executable = try_.DEFAULT_EXECUTABLE,
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = None,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium",
    branch_selector = [
        branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
        branches.FUCHSIA_LTS_MILESTONE,
    ],
)

try_.builder(
    name = "android-official",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/android-official",
    ],
)

try_.builder(
    name = "fuchsia-official",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    mirrors = [
        "ci/fuchsia-official",
    ],
)

try_.builder(
    name = "linux-official",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/linux-official",
    ],
)

try_.builder(
    name = "mac-official",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    mirrors = [
        "ci/mac-official",
    ],
    cores = None,
    os = os.MAC_ANY,
    # TODO(crbug.com/1279290) builds with PGO change take long time.
    # Keep in sync with mac-official in ci/chromium.star.
    execution_timeout = 9 * time.hour,
    goma_backend = goma.backend.RBE_PROD,
    reclient_instance = None,
)

try_.builder(
    name = "win-official",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    mirrors = [
        "ci/win-official",
    ],
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "win32-official",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    mirrors = [
        "ci/win32-official",
    ],
    os = os.WINDOWS_DEFAULT,
    execution_timeout = 6 * time.hour,
)
