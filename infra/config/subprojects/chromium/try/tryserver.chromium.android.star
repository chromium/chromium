# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.android builder group."""

load("@chromium-luci//branches.star", "branches")
load("@chromium-luci//builder_config.star", "builder_config")
load("@chromium-luci//builders.star", "os")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//gn_args.star", "gn_args")
load("@chromium-luci//targets.star", "targets")
load("@chromium-luci//try.star", "try_")
load("//lib/gpu.star", "gpu")
load("//lib/siso.star", "siso")
load("//lib/try_constants.star", "try_constants")
load("//project.star", "settings")

try_.defaults.set(
    executable = try_constants.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.android",
    pool = try_constants.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 32,
    execution_timeout = try_constants.DEFAULT_EXECUTION_TIMEOUT,
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    orchestrator_cores = 4,
    service_account = try_constants.DEFAULT_SERVICE_ACCOUNT,
    # crbug.com/372192123 - downloading with "minimum" strategy doesn't work
    # well for Android builds because some steps have additional inputs/outputs
    # they are not configured in the build graph.
    siso_output_local_strategy = "greedy",
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
    siso_remote_linking = True,
)

targets.builder_defaults.set(
    mixins = [
        "chromium-tester-service-account",
    ],
)

consoles.list_view(
    name = "tryserver.chromium.android",
    branch_selector = branches.selector.ANDROID_BRANCHES,
)

try_.builder(
    name = "android-10-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-10-arm64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-10-arm64-rel",
            "release_try_builder",
        ],
    ),
    builderless = False,
    cores = 16,
    contact_team_email = "clank-engprod@google.com",
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/android/features/vr/.+",
            "chrome/android/java/src/org/chromium/chrome/browser/vr/.+",
            "chrome/android/javatests/src/org/chromium/chrome/browser/vr/.+",
            "chrome/browser/android/vr/.+",
            "chrome/browser/vr/.+",
            "components/webxr/.+",
            "content/browser/xr/.+",
            "device/vr/.+",
            "third_party/cardboard/.+",
            "third_party/openxr/.+",
            "third_party/gvr-android-sdk/.+",
            "third_party/arcore-android-sdk/.+",
            "third_party/arcore-android-sdk-client/.+",
        ],
    ),
)

try_.builder(
    name = "android-10-x86-rel",
    mirrors = [
        "ci/android-10-x86-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-10-x86-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-10-x86-fyi-rel",
    mirrors = [
        "ci/android-10-x86-fyi-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-10-x86-fyi-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-10-x86-nofieldtrial-rel",
    mirrors = [
        "ci/android-10-x86-nofieldtrial-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-10-x86-nofieldtrial-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-11-x86-rel",
    mirrors = [
        "ci/android-11-x86-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-11-x86-rel",
            "release_try_builder",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-12-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-12-x64-dbg-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Android x64 Builder (dbg)",
            "debug_try_builder",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-12-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-12-x64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-12-x64-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-12l-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-12l-x64-dbg-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Android x64 Builder (dbg)",
            "debug_try_builder",
        ],
    ),
)

try_.builder(
    name = "android-12l-landscape-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-12l-landscape-x64-dbg-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Android x64 Builder (dbg)",
            "debug_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-12l-x64-rel-cq",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-12l-x64-rel-cq",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-12l-x64-rel-cq",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-13-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-13-x64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-13-x64-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-13-x64-fyi-rel",
    mirrors = [
        "ci/android-13-x64-fyi-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-13-x64-fyi-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-14-tablet-landscape-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-14-tablet-landscape-arm64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-14-tablet-landscape-arm64-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-14-arm64-fyi-rel",
    mirrors = [
        "ci/android-14-arm64-fyi-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-14-arm64-fyi-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    coverage_test_types = ["unit", "overall"],
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    use_clang_coverage = True,
)

try_.builder(
    name = "android-14-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-14-arm64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-14-arm64-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    coverage_test_types = ["unit", "overall"],
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    use_clang_coverage = True,
)

try_.builder(
    name = "android-14-automotive-landscape-x64-rel",
    mirrors = [
        "ci/android-14-automotive-landscape-x64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-14-automotive-landscape-x64-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-14-x64-rel",
    mirrors = [
        "ci/android-14-x64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-14-x64-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-14-x64-fyi-rel",
    mirrors = [
        "ci/android-14-x64-fyi-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-14-x64-fyi-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-15-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Chromium tests on Android 15 emulator.",
    mirrors = [
        "ci/android-15-x64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-15-x64-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-15-tablet-landscape-x64-rel",
    # TODO(crbug.com/376748979 ): Enable on branches once tests are stable
    # branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Chromium tests on Android 15 tablet landscape emulator.",
    mirrors = [
        "ci/android-15-tablet-landscape-x64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-15-tablet-landscape-x64-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-15-tablet-x64-rel",
    # TODO(crbug.com/376748979 ): Enable on branches once tests are stable
    # branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Chromium tests on Android 15 tablet emulator.",
    mirrors = [
        "ci/android-15-tablet-x64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-15-tablet-x64-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-15-x64-fyi-rel",
    mirrors = [
        "ci/android-15-x64-fyi-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-15-x64-fyi-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-16-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-16-x64-dbg-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Android x64 Builder (dbg)",
            "debug_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-16-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Chromium tests on Android 16 emulator.",
    mirrors = [
        "ci/android-16-x64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-16-x64-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-16-x64-fyi-rel",
    mirrors = [
        "ci/android-16-x64-fyi-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-16-x64-fyi-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-arm-compile-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = ["ci/Android arm Builder (dbg)"],
    gn_args = "ci/Android arm Builder (dbg)",
    contact_team_email = "clank-engprod@google.com",
)

try_.builder(
    name = "android-arm64-compile-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = ["ci/Android arm64 Builder (dbg)"],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "debug_try_builder",
            "enable_android_secondary_abi",
            "remoteexec",
            "compile_only",
            "arm64",
            "android_fastbuild",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
)

try_.orchestrator_builder(
    name = "android-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "This builder may trigger tests on multiple Android versions.",
    mirrors = [
        "ci/Android Release (Pixel 2)",  # Pixel 2 on Pie
        "ci/android-14-arm64-rel",  # Pixel 7 on Android 14
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-14-arm64-rel",
            "release_try_builder",
            "android_fastbuild",
            "enable_android_secondary_abi",
            "fail_on_android_expectations",
            "use_clang_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    compilator = "android-arm64-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
        # crbug/940930
        "chromium.enable_cleandead": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/40241638): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "android-arm64-rel-compilator",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    main_list_view = "try",
)

try_.builder(
    name = "android-mte-arm64-rel",
    mirrors = [
        "ci/android-mte-arm64-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-mte-arm64-rel",
            "release_try_builder",
            "minimal_symbols",
        ],
    ),
    contact_team_email = "chrome-mte@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-bfcache-rel",
    mirrors = [
        "ci/android-bfcache-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-bfcache-rel",
            "release_try_builder",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-binary-size",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    executable = "recipe:binary_size_trybot",
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "arm64",
            "chrome_with_codecs",
            "remoteexec",
            "minimal_symbols",
            "official_optimize",
            "stable_channel",
            "v8_release_branch",
            # Allows the bot to measure low-end arm32 and high-end arm64 using
            # a single build.
            "android_low_end_secondary_toolchain",
        ],
    ),
    builderless = not settings.is_main,
    cores = 32,
    ssd = True,
    main_list_view = "try",
    properties = {
        "$build/binary_size": {
            "analyze_targets": [
                "//chrome/android:trichrome_32_minimal_apks",
                "//chrome/android:trichrome_library_64_apk",
                "//chrome/android:validate_expectations",
                "//tools/binary_size:binary_size_trybot_py",
            ],
            "arm64_size_config_json": "config/TrichromeLibrary64_size_config.json",
            "compile_targets": [
                "check_chrome_static_initializers",
                "trichrome_32_minimal_apks",
                "trichrome_library_64_apk",
                "validate_expectations",
            ],
        },
    },
    # Do not use remote linking becaues it doesn't download
    # the remote artifacts by default. But, the binary size recipe
    # requires them for binary size check.
    # TODO: crbug.com/376354860 - Enable remote linking with an appropriate
    # download strategy so that we can downgrade the machine spec.
    siso_remote_linking = False,
    tryjob = try_.job(),
)

try_.builder(
    name = "android-canary-x64-fyi-rel",
    mirrors = [
        "ci/android-canary-x64-fyi-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-canary-x64-fyi-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-clobber-rel",
    mirrors = [
        "ci/android-archive-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-archive-rel",
            "release_try_builder",
            "chrome_with_codecs",
        ],
    ),
    contact_team_email = "clank-engprod@google.com",
)

try_.builder(
    name = "android-cronet-arm-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-cronet-arm-dbg",
    ],
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "arm",
            "cronet_android",
            "debug_static_builder",
            "remoteexec",
            "release_java",
        ],
    ),
    contact_team_email = "cronet-team@google.com",
    main_list_view = "try",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "components/cronet/.+",
            "components/grpc_support/.+",
            "build/android/.+",
            "build/config/android/.+",
            "third_party/jni_zero/.+",
        ],
    ),
)

try_.builder(
    name = "android-cronet-arm-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Compile Cronet targets and verify the sizes",
    mirrors = [
        "ci/android-cronet-arm-rel",
    ],
    builder_config_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "arm",
            "cronet_android",
            "release_try_builder",
            "remoteexec",
        ],
    ),
    builderless = not settings.is_main,
    contact_team_email = "cronet-team@google.com",
    experiments = {
        # crbug/940930
        "chromium.enable_cleandead": 50,
    },
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "android-cronet-arm64-dbg",
    mirrors = ["ci/android-cronet-arm64-dbg"],
    gn_args = "ci/android-cronet-arm64-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-arm64-rel",
    mirrors = ["ci/android-cronet-arm64-rel"],
    # TODO(crbug.com/40462241): Switch this back to debug try builder when cronet's
    # shared library loading is fixed.
    gn_args = "ci/android-cronet-arm64-rel",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-riscv64-dbg",
    mirrors = ["ci/android-cronet-riscv64-dbg"],
    gn_args = "ci/android-cronet-riscv64-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-riscv64-rel",
    mirrors = ["ci/android-cronet-riscv64-rel"],
    gn_args = "ci/android-cronet-riscv64-rel",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x64-rel",
    mirrors = ["ci/android-cronet-x64-rel"],
    gn_args = "ci/android-cronet-x64-rel",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "components/cronet/.+",
            "components/grpc_support/.+",
            "build/android/.+",
            "build/config/android/.+",
            "third_party/jni_zero/.+",
        ],
    ),
)

try_.builder(
    name = "android-cronet-x64-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = ["ci/android-cronet-x64-dbg"],
    gn_args = "ci/android-cronet-x64-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x64-dbg-12-tests",
    mirrors = [
        "ci/android-cronet-x64-dbg",
        "ci/android-cronet-x64-dbg-12-tests",
    ],
    gn_args = "ci/android-cronet-x64-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x64-dbg-13-tests",
    mirrors = [
        "ci/android-cronet-x64-dbg",
        "ci/android-cronet-x64-dbg-13-tests",
    ],
    gn_args = "ci/android-cronet-x64-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x64-dbg-14-tests",
    description_html = "Tests Cronet against Android 14",
    mirrors = [
        "ci/android-cronet-x64-dbg",
        "ci/android-cronet-x64-dbg-14-tests",
    ],
    gn_args = "ci/android-cronet-x64-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x64-dbg-15-tests",
    description_html = "Tests Cronet against Android 15",
    mirrors = [
        "ci/android-cronet-x64-dbg",
        "ci/android-cronet-x64-dbg-15-tests",
    ],
    gn_args = "ci/android-cronet-x64-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x64-dbg-16-tests",
    description_html = "Tests Cronet against Android 16",
    mirrors = [
        "ci/android-cronet-x64-dbg",
        "ci/android-cronet-x64-dbg-16-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-cronet-x64-dbg",
            "use_clang_coverage",
            "use_java_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    contact_team_email = "cronet-team@google.com",
    coverage_test_types = ["unit", "overall"],
    main_list_view = "try",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "components/cronet/.+",
            "components/grpc_support/.+",
            "build/android/.+",
            "build/config/android/.+",
        ],
    ),
    use_clang_coverage = True,
    use_java_coverage = True,
)

try_.builder(
    name = "android-cronet-x86-dbg",
    mirrors = ["ci/android-cronet-x86-dbg"],
    gn_args = "ci/android-cronet-x86-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-rel",
    mirrors = ["ci/android-cronet-x86-rel"],
    gn_args = "ci/android-cronet-x86-rel",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-10-tests",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-10-tests",
    ],
    gn_args = gn_args.config(
        configs = ["ci/android-cronet-x86-dbg"],
    ),
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-11-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-11-tests",
    ],
    gn_args = "ci/android-cronet-x86-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-oreo-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-oreo-tests",
    ],
    gn_args = "ci/android-cronet-x86-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-pie-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-pie-tests",
    ],
    gn_args = "ci/android-cronet-x86-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-nougat-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-nougat-tests",
    ],
    gn_args = "ci/android-cronet-x86-dbg",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-code-coverage-native",
    mirrors = ["ci/android-cronet-code-coverage-native"],
    gn_args = "ci/android-cronet-code-coverage-native",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-code-coverage-java",
    mirrors = ["ci/android-cronet-code-coverage-java"],
    gn_args = "ci/android-cronet-code-coverage-java",
    contact_team_email = "cronet-team@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-marshmallow-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-marshmallow-tests",
    ],
    # Replicates "ci/android-cronet-x86-dbg", with code coverage related
    # arguments appended.
    gn_args = gn_args.config(
        configs = [
            "ci/android-cronet-x86-dbg",
            "use_clang_coverage",
            "use_java_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    contact_team_email = "cronet-team@google.com",
    coverage_test_types = ["unit", "overall"],
    main_list_view = "try",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "components/cronet/.+",
            "components/grpc_support/.+",
            "build/android/.+",
            "build/config/android/.+",
        ],
    ),
    use_clang_coverage = True,
    use_java_coverage = True,
)

try_.builder(
    name = "android-deterministic-dbg",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_builder",
            "remoteexec",
            "arm",
        ],
    ),
    execution_timeout = 6 * time.hour,
    # Do not use remote linking becaues it doesn't download
    # the remote artifacts by default. But, the deterministic recipe
    # requires them for determinism check.
    siso_remote_linking = False,
)

try_.builder(
    name = "android-deterministic-rel",
    executable = "recipe:swarming/deterministic_build",
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_with_static_analysis",
            "release_try_builder",
            "remoteexec",
            "strip_debug_info",
            "arm",
        ],
    ),
    execution_timeout = 6 * time.hour,
    # Do not use remote linking becaues it doesn't download
    # the remote artifacts by default. But, the deterministic recipe
    # requires them for determinism check.
    siso_remote_linking = False,
)

try_.builder(
    name = "android-fieldtrial-rel",
    mirrors = ["ci/android-fieldtrial-rel"],
    gn_args = gn_args.config(
        configs = [
            "ci/android-fieldtrial-rel",
            "try_builder",
            "no_symbols",
        ],
    ),
)

try_.builder(
    name = "android-perfetto-rel",
    mirrors = [
        "ci/android-perfetto-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-perfetto-rel",
            "try_builder",
            "no_symbols",
        ],
    ),
)

try_.builder(
    name = "android-webview-10-x86-rel-tests",
    mirrors = [
        "ci/android-x86-rel",
        "ci/android-webview-10-x86-rel-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-x86-rel",
            "release_try_builder",
        ],
    ),
)

try_.builder(
    name = "android-15-chrome-wpt-fyi-rel",
    mirrors = ["ci/android-15-chrome-wpt-fyi-rel"],
    gn_args = "ci/android-15-chrome-wpt-fyi-rel",
    contact_team_email = "chrome-product-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-15-webview-wpt-fyi-rel",
    mirrors = ["ci/android-15-webview-wpt-fyi-rel"],
    gn_args = "ci/android-15-webview-wpt-fyi-rel",
    contact_team_email = "chrome-product-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

# Temporary builder for checking structured test ids.
try_.builder(
    name = "android-structured-test-ids-16-x64-rel-fyi",
    mirrors = [
        "ci/android-structured-test-ids-16-x64-rel-fyi",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-structured-test-ids-16-x64-rel-fyi",
            "release_try_builder",
        ],
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
    experiments = {
        "chromium_tests.resultdb_module": 100,
    },
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-12-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-webview-12-x64-dbg-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Android x64 Builder (dbg)",
            "debug_try_builder",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-13-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-webview-13-x64-dbg-tests",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/Android x64 Builder (dbg)",
            "debug_try_builder",
        ],
    ),
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-13-x64-hostside-rel",
    mirrors = [
        "ci/android-webview-13-x64-hostside-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-webview-13-x64-hostside-rel",
            "release_try_builder",
        ],
    ),
    contact_team_email = "woa-engprod@google.com",
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "android-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Run Chromium tests on Android emulators.",
    mirrors = [
        "ci/android-12l-x64-rel-cq",
        "ci/android-15-x64-rel",
        "ci/android-webview-13-x64-hostside-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-15-x64-rel",
            "release_try_builder",
            "use_clang_coverage",
            "use_java_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    compilator = "android-x64-rel-compilator",
    contact_team_email = "clank-engprod@google.com",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
        # crbug/940930
        "chromium.enable_cleandead": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/40241638): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    use_clang_coverage = True,
    use_java_coverage = True,
)

try_.compilator_builder(
    name = "android-x64-rel-compilator",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Compilator builder for android-x64-rel",
    contact_team_email = "clank-engprod@google.com",
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "android-x86-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-10-x86-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            "ci/android-10-x86-rel",
            "release_try_builder",
            "use_clang_coverage",
            "use_java_coverage",
            "partial_code_coverage_instrumentation",
        ],
    ),
    compilator = "android-x86-rel-compilator",
    contact_team_email = "clank-engprod@google.com",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        "chromium.add_one_test_shard": 10,
        # crbug/940930
        "chromium.enable_cleandead": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/40241638): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    use_clang_coverage = True,
    use_java_coverage = True,
)

try_.compilator_builder(
    name = "android-x86-rel-compilator",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    cores = 64 if settings.is_main else 32,
    contact_team_email = "clank-engprod@google.com",
    main_list_view = "try",
)

try_.builder(
    name = "android_blink_rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "release_try_builder",
            "remoteexec",
            "strip_debug_info",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "chromium_android_webkit_gtests",
        ],
        mixins = [
            "12-x64-emulator",
            "emulator-8-cores",
            "linux-jammy",
            "x86-64",
        ],
    ),
    targets_settings = targets.settings(
        os_type = targets.os_type.ANDROID,
    ),
)

try_.builder(
    name = "android-cast-arm-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-cast-arm-dbg",
    ],
    gn_args = "ci/android-cast-arm-dbg",
    contact_team_email = "cast-eng@google.com",
    tryjob = try_.job(
        location_filters = [
            "build/android/.+",
            "build/config/android/.+",
            "chrome/browser/safe_browsing/BUILD.gn",
            "chromecast/.+",
            "components/cast/.+",
            "components/cast_receiver/.+",
            "components/cast_streaming/.+",
            "third_party/cast_core/.+",
            "third_party/openscreen/.+",
        ],
    ),
)

try_.builder(
    name = "android-cast-arm-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-cast-arm-rel",
    ],
    gn_args = "ci/android-cast-arm-rel",
    contact_team_email = "cast-eng@google.com",
    tryjob = try_.job(
        location_filters = [
            "build/android/.+",
            "build/config/android/.+",
            "chrome/browser/safe_browsing/BUILD.gn",
            "chromecast/.+",
            "components/cast/.+",
            "components/cast_receiver/.+",
            "components/cast_streaming/.+",
            "third_party/cast_core/.+",
            "third_party/openscreen/.+",
        ],
    ),
)

try_.builder(
    name = "android-cast-arm64-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-cast-arm64-dbg",
    ],
    gn_args = "ci/android-cast-arm64-dbg",
    contact_team_email = "cast-eng@google.com",
    tryjob = try_.job(
        location_filters = [
            "build/android/.+",
            "build/config/android/.+",
            "chrome/browser/safe_browsing/BUILD.gn",
            "chromecast/.+",
            "components/cast/.+",
            "components/cast_receiver/.+",
            "components/cast_streaming/.+",
            "third_party/cast_core/.+",
            "third_party/jni_zero/.+",
            "third_party/openscreen/.+",
        ],
    ),
)

try_.builder(
    name = "android-cast-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-cast-arm64-rel",
    ],
    gn_args = "ci/android-cast-arm64-rel",
    contact_team_email = "cast-eng@google.com",
    tryjob = try_.job(
        location_filters = [
            "build/android/.+",
            "build/config/android/.+",
            "chrome/browser/safe_browsing/BUILD.gn",
            "chromecast/.+",
            "components/cast/.+",
            "components/cast_receiver/.+",
            "components/cast_streaming/.+",
            "third_party/cast_core/.+",
            "third_party/jni_zero/.+",
            "third_party/openscreen/.+",
        ],
    ),
)

try_.builder(
    name = "android_compile_dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/Android arm64 Builder All Targets (dbg)",
    ],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_try_builder",
            "enable_android_secondary_abi",
            "remoteexec",
            "compile_only",
            "arm64",
        ],
    ),
    builderless = not settings.is_main,
    cores = 32 if settings.is_main else 16,
    ssd = True,
    experiments = {
        # crbug/940930
        "chromium.enable_cleandead": 100,
    },
    main_list_view = "try",
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(),
)

try_.builder(
    name = "android_compile_x64_dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    # Since we expect this builder to compile all, let it mirror
    # "Android x64 Builder All Targets (dbg)" rather than
    # "Android x64 Builder (dbg)"
    mirrors = [
        "ci/Android x64 Builder All Targets (dbg)",
    ],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_try_builder",
            "remoteexec",
            "compile_only",
            "x64",
        ],
    ),
    cores = 16,
    ssd = True,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/android/java/src/org/chromium/chrome/browser/vr/.+",
            "chrome/browser/vr/.+",
            "content/browser/xr/.+",
            "sandbox/linux/seccomp-bpf/.+",
            "sandbox/linux/seccomp-bpf-helpers/.+",
            "sandbox/linux/system_headers/.+",
            "sandbox/linux/tests/.+",
            # Diectories that have caused breakages in the past due to the
            # TensorFlowLite roll.
            "third_party/eigen3/.+",
            "third_party/farmhash/.+",
            "third_party/fft2d/.+",
            "third_party/flatbuffers/.+",
            "third_party/fp16/.+",
            "third_party/fxdiv/.+",
            "third_party/gemmlowp/.+",
            "third_party/gvr-android-sdk/.+",
            "third_party/pthreadpool/.+",
            "third_party/ruy/.+",
            "third_party/tflite/.+",
            "third_party/xnnpack/.+",
        ],
    ),
)

try_.builder(
    name = "android_compile_x86_dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/Android x86 Builder (dbg)",
    ],
    builder_config_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder",
            "android_with_static_analysis",
            "debug_try_builder",
            "remoteexec",
            "compile_only",
            "x86",
        ],
    ),
    cores = 16,
    ssd = True,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/android/java/src/org/chromium/chrome/browser/vr/.+",
            "chrome/browser/vr/.+",
            "content/browser/xr/.+",
            "sandbox/linux/seccomp-bpf/.+",
            "sandbox/linux/seccomp-bpf-helpers/.+",
            "sandbox/linux/system_headers/.+",
            "sandbox/linux/tests/.+",
            "third_party/gvr-android-sdk/.+",
        ],
    ),
)

gpu.try_.optional_tests_builder(
    name = "android_optional_gpu_tests_rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Runs GPU tests on Pixel 4 devices. Only automatically added to CLs that touch GPU-related files.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "main_builder",
            build_config = builder_config.build_config.DEBUG,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "base_config",
        ),
    ),
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "gpu_tests",
            "android_builder",
            "android_with_static_analysis",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "dcheck_always_on",
            "static_angle",
            "arm",
        ],
    ),
    targets = targets.bundle(
        targets = [
            "gpu_pixel_04_telemetry_tests",
            "android_webview_gpu_telemetry_tests",
        ],
        mixins = [
            "has_native_resultdb_integration",
            "gpu_pixel_4_stable",
        ],
        per_test_modifications = {
            "expected_color_pixel_passthrough_ganesh_test": targets.mixin(
                args = [
                    # See Android FYI Release (Pixel 4).
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
            "expected_color_pixel_validating_test": targets.mixin(
                args = [
                    # See Android FYI Release (Pixel 4).
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
            "pixel_skia_gold_passthrough_ganesh_test": targets.mixin(
                args = [
                    # See Android FYI Release (Pixel 4).
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
            "pixel_skia_gold_validating_test": targets.mixin(
                args = [
                    # See Android FYI Release (Pixel 4).
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
            "screenshot_sync_passthrough_ganesh_tests": targets.mixin(
                args = [
                    # See Android FYI Release (Pixel 4).
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
            "screenshot_sync_validating_tests": targets.mixin(
                args = [
                    # See Android FYI Release (Pixel 4).
                    "--extra-browser-args=--disable-wcg-for-test",
                ],
            ),
        },
    ),
    targets_settings = targets.settings(
        browser_config = targets.browser_config.ANDROID_CHROMIUM,
        os_type = targets.os_type.ANDROID,
        use_android_merge_script_by_default = False,
    ),
    pool = "luci.chromium.gpu.try",
    builderless = True,
    ssd = None,
    free_space = None,
    alerts_enabled = False,
    contact_team_email = "chrome-gpu-infra@google.com",
    main_list_view = "try",
    max_concurrent_builds = 10,
    tryjob = try_.job(
        location_filters = [
            # Inclusion filters.
            cq.location_filter(path_regexp = "cc/.+"),
            cq.location_filter(path_regexp = "chrome/browser/vr/.+"),
            cq.location_filter(path_regexp = "content/browser/xr/.+"),
            cq.location_filter(path_regexp = "components/viz/.+"),
            cq.location_filter(path_regexp = "content/test/data/gpu/.+"),
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "media/audio/.+"),
            cq.location_filter(path_regexp = "media/base/.+"),
            cq.location_filter(path_regexp = "media/capture/.+"),
            cq.location_filter(path_regexp = "media/filters/.+"),
            cq.location_filter(path_regexp = "media/gpu/.+"),
            cq.location_filter(path_regexp = "media/mojo/.+"),
            cq.location_filter(path_regexp = "media/renderers/.+"),
            cq.location_filter(path_regexp = "media/video/.+"),
            cq.location_filter(path_regexp = "services/viz/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/tryserver.chromium.android.json"),
            cq.location_filter(path_regexp = "testing/trigger_scripts/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/mediastream/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webcodecs/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgl/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "ui/gl/.+"),

            # Exclusion filters.
            cq.location_filter(exclude = True, path_regexp = ".*\\.md"),
        ],
    ),
)

gpu.try_.optional_tests_builder(
    name = "gpu-fyi-cq-android-arm64",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "Runs GPU tests on Pixel 6 devices. Only automatically added to CLs that touch GPU-related files.",
    mirrors = [
        "ci/GPU FYI Android arm64 Builder",
        "ci/Android FYI Release (Pixel 6)",
    ],
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = "ci/GPU FYI Android arm64 Builder",
    pool = "luci.chromium.gpu.try",
    builderless = True,
    ssd = None,
    free_space = None,
    # Exclude gpu fyi builders.
    alerts_enabled = False,
    contact_team_email = "chrome-gpu-infra@google.com",
    main_list_view = "try",
    max_concurrent_builds = 10,
    tryjob = try_.job(
        location_filters = [
            # Inclusion filters.
            cq.location_filter(path_regexp = "cc/.+"),
            cq.location_filter(path_regexp = "chrome/browser/vr/.+"),
            cq.location_filter(path_regexp = "content/browser/xr/.+"),
            cq.location_filter(path_regexp = "components/viz/.+"),
            cq.location_filter(path_regexp = "content/test/data/gpu/.+"),
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "media/audio/.+"),
            cq.location_filter(path_regexp = "media/base/.+"),
            cq.location_filter(path_regexp = "media/capture/.+"),
            cq.location_filter(path_regexp = "media/filters/.+"),
            cq.location_filter(path_regexp = "media/gpu/.+"),
            cq.location_filter(path_regexp = "media/mojo/.+"),
            cq.location_filter(path_regexp = "media/renderers/.+"),
            cq.location_filter(path_regexp = "media/video/.+"),
            cq.location_filter(path_regexp = "services/viz/.+"),
            cq.location_filter(path_regexp = "testing/trigger_scripts/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/mediastream/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webcodecs/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgl/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "ui/gl/.+"),

            # Exclusion filters.
            cq.location_filter(exclude = True, path_regexp = ".*\\.md"),
        ],
    ),
)

try_.builder(
    name = "android-code-coverage",
    mirrors = ["ci/android-code-coverage"],
    gn_args = "ci/android-code-coverage",
    execution_timeout = 20 * time.hour,
)

try_.builder(
    name = "android-code-coverage-native",
    mirrors = ["ci/android-code-coverage-native"],
    gn_args = "ci/android-code-coverage-native",
    execution_timeout = 20 * time.hour,
)

try_.builder(
    name = "android-x86-code-coverage",
    mirrors = ["ci/android-x86-code-coverage"],
    gn_args = "ci/android-x86-code-coverage",
    contact_team_email = "clank-engprod@google.com",
    execution_timeout = 20 * time.hour,
)
