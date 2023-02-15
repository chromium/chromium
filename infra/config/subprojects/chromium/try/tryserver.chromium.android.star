# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.android builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "reclient")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "settings")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.android",
    pool = try_.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 32,
    compilator_goma_jobs = goma.jobs.J300,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    orchestrator_cores = 4,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.android",
    branch_selector = branches.selector.ANDROID_BRANCHES,
)

try_.builder(
    name = "android-10-arm64-rel",
    mirrors = [
        "ci/android-10-arm64-rel",
    ],
    goma_backend = None,
)

try_.builder(
    name = "android-11-x86-rel",
    mirrors = [
        "ci/android-11-x86-rel",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-12-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-12-x64-dbg-tests",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "android-12-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-12-x64-rel",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    compilator = "android-12-x64-rel-compilator",
    experiments = {
        "chromium_rts.inverted_rts": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "android-12-x64-rel-compilator",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    main_list_view = "try",
)

try_.builder(
    name = "android-12l-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-12l-x64-dbg-tests",
    ],
)

try_.orchestrator_builder(
    name = "android-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "This builder may trigger tests on multiple Android versions.",
    mirrors = [
        "ci/Android Release (Nexus 5X)",  # Nexus 5X on Nougat
        "ci/android-pie-arm64-rel",  # Pixel 1, 2 on Pie
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    check_for_flakiness = True,
    compilator = "android-arm64-rel-compilator",
    experiments = {
        "chromium_rts.inverted_rts": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.orchestrator_builder(
    name = "android-arm64-rel-inverse-fyi",
    mirrors = [
        "ci/Android Release (Nexus 5X)",  # Nexus 5X on Nougat
        "ci/android-pie-arm64-rel",  # Pixel 1, 2 on Pie
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    check_for_flakiness = True,
    compilator = "android-arm64-rel-compilator",
    experiments = {
        "chromium_rts.inverted_rts": 100,
        "chromium_rts.inverted_rts_bail_early": 100,
    },
    use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "android-arm64-rel-compilator",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    check_for_flakiness = True,
    main_list_view = "try",
)

try_.builder(
    name = "android-asan",
    mirrors = ["ci/android-asan"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-bfcache-rel",
    mirrors = [
        "ci/android-bfcache-rel",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-binary-size",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    executable = "recipe:binary_size_trybot",
    builderless = not settings.is_main,
    cores = 16,
    ssd = True,
    goma_backend = None,
    main_list_view = "try",

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
    properties = {
        "$build/binary_size": {
            "analyze_targets": [
                "//chrome/android:monochrome_public_minimal_apks",
                "//chrome/android:trichrome_minimal_apks",
                "//chrome/android:validate_expectations",
                "//tools/binary_size:binary_size_trybot_py",
            ],
            "compile_targets": [
                "monochrome_public_minimal_apks",
                "monochrome_static_initializers",
                "trichrome_minimal_apks",
                "validate_expectations",
            ],
        },
    },
    tryjob = try_.job(),
)

try_.builder(
    name = "android-cronet-arm-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-cronet-arm-dbg",
    ],
    goma_backend = None,
    main_list_view = "try",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "components/cronet/.+",
            "components/grpc_support/.+",
            "build/android/.+",
            "build/config/android/.+",
            cq.location_filter(exclude = True, path_regexp = "components/cronet/ios/.+"),
        ],
    ),
)

try_.builder(
    name = "android-cronet-arm64-dbg",
    mirrors = ["ci/android-cronet-arm64-dbg"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-arm64-rel",
    mirrors = ["ci/android-cronet-arm64-rel"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-asan-arm-rel",
    mirrors = ["ci/android-cronet-asan-arm-rel"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg",
    mirrors = [
        "ci/android-cronet-x86-dbg",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-rel",
    mirrors = ["ci/android-cronet-x86-rel"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-10-tests",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-10-tests",
    ],
    check_for_flakiness = True,
    goma_backend = None,
    main_list_view = "try",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "components/cronet/.+",
            "components/grpc_support/.+",
            "build/android/.+",
            "build/config/android/.+",
            cq.location_filter(exclude = True, path_regexp = "components/cronet/ios/.+"),
        ],
    ),
)

try_.builder(
    name = "android-cronet-x86-dbg-11-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-11-tests",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-oreo-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-oreo-tests",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-pie-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-pie-tests",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-rel-kitkat-tests",
    mirrors = [
        "ci/android-cronet-x86-rel",
        "ci/android-cronet-x86-rel-kitkat-tests",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-deterministic-dbg",
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
)

try_.builder(
    name = "android-deterministic-rel",
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
)

try_.builder(
    name = "android-fieldtrial-rel",
    mirrors = ["ci/android-fieldtrial-rel"],
)

try_.builder(
    name = "android-inverse-fieldtrials-pie-x86-fyi-rel",
    mirrors = builder_config.copy_from("try/android-pie-x86-rel"),
)

try_.orchestrator_builder(
    name = "android-nougat-x86-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-nougat-x86-rel",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    check_for_flakiness = True,
    compilator = "android-nougat-x86-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        "chromium_rts.inverted_rts": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    use_java_coverage = True,
)

try_.orchestrator_builder(
    name = "android-nougat-x86-rel-inverse-fyi",
    mirrors = [
        "ci/android-nougat-x86-rel",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    check_for_flakiness = True,
    compilator = "android-nougat-x86-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        "chromium_rts.inverted_rts": 100,
        "chromium_rts.inverted_rts_bail_early": 100,
    },
    use_java_coverage = True,
    use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "android-nougat-x86-rel-compilator",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    cores = 64 if settings.is_main else 32,
    check_for_flakiness = True,
    main_list_view = "try",
)

try_.builder(
    name = "android-oreo-arm64-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Oreo Phone Tester",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-perfetto-rel",
    mirrors = [
        "ci/android-perfetto-rel",
    ],
)

try_.builder(
    name = "android-pie-arm64-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/android-pie-arm64-dbg",
    ],
    builderless = False,
    cores = 16,
    check_for_flakiness = True,
    goma_backend = None,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/android/features/vr/.+",
            "chrome/android/java/src/org/chromium/chrome/browser/vr/.+",
            "chrome/android/javatests/src/org/chromium/chrome/browser/vr/.+",
            "chrome/browser/android/vr/.+",
            "chrome/browser/vr/.+",
            "content/browser/xr/.+",
            "device/vr/android/.+",
            "third_party/gvr-android-sdk/.+",
            "third_party/arcore-android-sdk/.+",
            "third_party/arcore-android-sdk-client/.+",
        ],
    ),
)

# TODO(crbug/1182468) Remove when experiment is done.
try_.builder(
    name = "android-pie-arm64-coverage-experimental-rel",
    mirrors = ["ci/android-pie-arm64-coverage-experimental-rel"],
    builderless = True,
    cores = 16,
    ssd = True,
    goma_backend = None,
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 3,
    ),
    use_clang_coverage = True,
)

try_.builder(
    name = "android-pie-x86-rel",
    mirrors = [
        "ci/android-pie-x86-rel",
    ],
    goma_backend = None,
)

# TODO(crbug/1182468) Remove when coverage is enabled on CQ.
try_.builder(
    name = "android-pie-arm64-coverage-rel",
    mirrors = ["ci/android-code-coverage-native"],
    cores = 16,
    ssd = True,
    goma_backend = None,
    use_clang_coverage = True,
)

try_.builder(
    name = "android-webview-10-x86-rel-tests",
    mirrors = [
        "ci/android-x86-rel",
        "ci/android-webview-10-x86-rel-tests",
    ],
    goma_backend = None,
)

try_.builder(
    name = "android-pie-arm64-wpt-rel-non-cq",
    mirrors = ["ci/android-pie-arm64-wpt-rel-non-cq"],
)

try_.builder(
    name = "android-chrome-pie-x86-wpt-fyi-rel",
    mirrors = ["ci/android-chrome-pie-x86-wpt-fyi-rel"],
)

try_.builder(
    name = "android-webview-12-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-webview-12-x64-dbg-tests",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-pie-x86-wpt-fyi-rel",
    mirrors = ["ci/android-webview-pie-x86-wpt-fyi-rel"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-nougat-arm64-dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView N (dbg)",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-oreo-arm64-dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView O (dbg)",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-pie-arm64-dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView P (dbg)",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android_archive_rel_ng",
    mirrors = [
        "ci/android-archive-rel",
    ],
)

try_.builder(
    name = "android_arm64_dbg_recipe",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    goma_backend = None,
)

try_.builder(
    name = "android-arm64-all-targets-dbg",
    mirrors = [
        "ci/Android arm64 Builder All Targets (dbg)",
    ],
    goma_backend = None,
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
            config = "android",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    goma_backend = None,
)

try_.builder(
    name = "android-x64-cast",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/Cast Android (dbg)",
    ],
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "android_compile_dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    builderless = not settings.is_main,
    cores = 32 if settings.is_main else 8,
    goma_backend = None,
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
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
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    cores = 16,
    ssd = True,
    goma_backend = None,
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

try_.builder(
    name = "android_compile_x86_dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/Android x86 Builder (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    cores = 16,
    ssd = True,
    goma_backend = None,
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

try_.builder(
    name = "android_cronet",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-cronet-arm-rel",
    ],
    try_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    builderless = not settings.is_main,
    goma_backend = None,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "android_unswarmed_pixel_aosp",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView N (dbg)",
    ],
)

try_.builder(
    name = "try-nougat-phone-tester",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Nougat Phone Tester",
    ],
    goma_backend = None,
)

try_.gpu.optional_tests_builder(
    name = "android_optional_gpu_tests_rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
        build_gs_bucket = "chromium-gpu-fyi-archive",
    ),
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    check_for_flakiness = True,
    goma_backend = None,
    goma_jobs = goma.jobs.J150,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "cc/.+"),
            cq.location_filter(path_regexp = "chrome/browser/vr/.+"),
            cq.location_filter(path_regexp = "content/browser/xr/.+"),
            cq.location_filter(path_regexp = "components/viz/.+"),
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
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "tools/mb/mb_config_expectations/tryserver.chromium.android.json"),
            cq.location_filter(path_regexp = "ui/gl/.+"),
        ],
    ),
)
