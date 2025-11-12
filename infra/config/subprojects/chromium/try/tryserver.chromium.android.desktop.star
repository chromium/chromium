# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.android builder group."""

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builders.star", "builders", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//try.star", "try_")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.android",
    pool = try_constants.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 32,
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    orchestrator_cores = 4,
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
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
    cores = 32,
    ssd = True,
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
    cores = 32,
    ssd = True,
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
    cores = 32,
    ssd = True,
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
            "use_clang_coverage",
            "use_java_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    compilator = "android-desktop-x64-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # crbug.com/40617829
        "chromium.enable_cleandead": 100,
    },
    main_list_view = "try",
    # TODO(crbug.com/40241638): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    tryjob = try_.job(),
    use_clang_coverage = True,
    use_java_coverage = True,
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
    cores = 32,
    ssd = True,
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
    cores = 32,
    ssd = True,
)

try_.builder(
    name = "android-desktop-arm64-deterministic-rel",
    description_html = "Deterministic arm64 release trybot for Android Desktop.",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-arm64-deterministic-rel",
        ],
    ),
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "android-desktop-arm64-deterministic-dbg",
    description_html = "Deterministic arm64 dbg build for Android Desktop.",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-arm64-deterministic-dbg",
        ],
    ),
    free_space = builders.free_space.high,
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "android-desktop-x64-deterministic-rel",
    description_html = "Deterministic x64 release trybot for Android Desktop.",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-x64-deterministic-rel",
        ],
    ),
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "android-desktop-x64-deterministic-dbg",
    description_html = "Deterministic x64 dbg build for Android Desktop.",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "ci/android-desktop-x64-deterministic-dbg",
        ],
    ),
    free_space = builders.free_space.high,
    execution_timeout = 6 * time.hour,
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

try_.builder(
    name = "android-desktop-arm64-binary-size",
    # TODO(crbug.com/439887309): Enable on ANDROID_BRANCHES
    #branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Measures binary size of android-desktop on arm64.",
    executable = "recipe:binary_size_trybot",
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "arm64",
            "chrome_with_codecs",
            "remoteexec",
            "minimal_symbols",
            "official_optimize",
            # TODO(crbug.com/433988303): Swap to stable.
            "dev_channel",
            "v8_release_branch",
        ],
    ),
    cores = 32,
    ssd = True,
    contact_team_email = "clank-engprod@google.com",
    main_list_view = "try",
    properties = {
        "$build/binary_size": {
            "analyze_targets": [
                "//chrome/android:trichrome_64_minimal_apks",
                "//chrome/android:trichrome_library_64_apk",
                "//chrome/android:validate_expectations",
                "//tools/binary_size:binary_size_trybot_py",
            ],
            "compile_targets": [
                "check_chrome_static_initializers",
                "trichrome_64_minimal_apks",
                "trichrome_library_64_apk",
                "validate_expectations",
            ],
        },
    },
    siso_remote_linking = False,
)

try_.builder(
    name = "android-desktop-x64-binary-size",
    # TODO(crbug.com/439887309): Enable on ANDROID_BRANCHES
    #branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Measures binary size of android-desktop on x64.",
    executable = "recipe:binary_size_trybot",
    gn_args = gn_args.config(
        configs = [
            "android_desktop",
            "android_builder",
            "x64",
            "chrome_with_codecs",
            "remoteexec",
            "minimal_symbols",
            "official_optimize",
            # TODO(crbug.com/433988303): Swap to stable.
            "dev_channel",
            "v8_release_branch",
        ],
    ),
    cores = 32,
    ssd = True,
    contact_team_email = "clank-engprod@google.com",
    main_list_view = "try",
    properties = {
        "$build/binary_size": {
            "analyze_targets": [
                "//chrome/android:trichrome_64_minimal_apks",
                "//chrome/android:trichrome_library_64_apk",
                "//chrome/android:validate_expectations",
                "//tools/binary_size:binary_size_trybot_py",
            ],
            "compile_targets": [
                "check_chrome_static_initializers",
                "trichrome_64_minimal_apks",
                "trichrome_library_64_apk",
                "validate_expectations",
            ],
        },
    },
    siso_remote_linking = False,
)
