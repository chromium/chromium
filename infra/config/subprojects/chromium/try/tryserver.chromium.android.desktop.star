# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.android builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "os", "siso")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.android",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    cores = 32,
    os = os.LINUX_DEFAULT,
    ssd = True,
    compilator_cores = 32,
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 4,
    reclient_enabled = False,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

consoles.list_view(
    name = "tryserver.chromium.android.desktop",
    branch_selector = branches.selector.MAIN,
)

try_.builder(
    name = "android-desktop-arm64-clobber-rel",
    mirrors = [
        "ci/android-desktop-arm64-archive-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-arm64-archive-rel",
            "release_try_builder",
            "chrome_with_codecs",
        ],
    ),
)

try_.builder(
    name = "android-desktop-x64-clobber-rel",
    mirrors = [
        "ci/android-desktop-x64-archive-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-x64-archive-rel",
            "release_try_builder",
            "chrome_with_codecs",
        ],
    ),
)

try_.builder(
    name = "android-desktop-arm64-compile-rel",
    mirrors = [
        "ci/android-desktop-arm64-compile-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-arm64-compile-rel",
            "release_try_builder",
        ],
    ),
)

try_.orchestrator_builder(
    name = "android-desktop-x64-rel",
    description_html = "Run Chromium tests on Android Desktop emulators.",
    mirrors = [
        "ci/android-desktop-x64-compile-rel",
        "ci/android-desktop-x64-rel-15-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-x64-compile-rel",
            "release_try_builder",
        ],
    ),
    compilator = "android-desktop-x64-rel-compilator",
    experiments = {
        # crbug.com/40617829
        "chromium.enable_cleandead": 100,
    },
    main_list_view = "try",
    # TODO(crbug.com/40241638): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "android-desktop-x64-rel-compilator",
    description_html = "Compilator builder for android-desktop-x64-rel",
    main_list_view = "try",
)

try_.builder(
    name = "android-desktop-arm64-compile-dbg",
    mirrors = [
        "ci/android-desktop-arm64-compile-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-arm64-compile-dbg",
            "debug_try_builder",
        ],
    ),
)

try_.builder(
    name = "android-desktop-x64-compile-dbg",
    mirrors = [
        "ci/android-desktop-x64-compile-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-x64-compile-dbg",
            "debug_try_builder",
        ],
    ),
)

try_.builder(
    name = "android-desktop-15-x64-rel",
    mirrors = [
        "ci/android-desktop-x64-compile-rel",
        "ci/android-desktop-x64-rel-15-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-x64-compile-rel",
            "release_try_builder",
        ],
    ),
)

try_.builder(
    name = "android-desktop-15-x64-fyi-rel",
    mirrors = [
        "ci/android-desktop-15-x64-fyi-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-15-x64-fyi-rel",
            "release_try_builder",
        ],
    ),
)
