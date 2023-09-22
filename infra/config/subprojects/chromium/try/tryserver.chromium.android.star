# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.android builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "siso")
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
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 4,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_configs = ["remote_all"],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
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
)

try_.builder(
    name = "android-11-x86-rel",
    mirrors = [
        "ci/android-11-x86-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-12-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-12-x64-dbg-tests",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "android-12-x64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-12-x64-rel",
    ],
    compilator = "android-12-x64-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    use_java_coverage = True,
)

try_.compilator_builder(
    name = "android-12-x64-rel-compilator",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "android-12-x64-siso-rel",
    description_html = """\
This builder shadows android-12-x64-rel builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating android-12-x64-rel from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/android-12-x64-rel"),
    compilator = "android-12-x64-siso-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    main_list_view = "try",
    tryjob = try_.job(
        # Decreasing the experiment percentage while enabling tests to reduce
        # extra workloads on the test pool.
        experiment_percentage = 10,
    ),
    use_java_coverage = True,
)

try_.compilator_builder(
    name = "android-12-x64-siso-rel-compilator",
    main_list_view = "try",
    siso_enabled = True,
)

try_.builder(
    name = "android-12l-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-12l-x64-dbg-tests",
    ],
)

try_.builder(
    name = "android-13-x64-rel",
    mirrors = [
        "ci/android-13-x64-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "android-arm64-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    description_html = "This builder may trigger tests on multiple Android versions.",
    mirrors = [
        "ci/Android Release (Nexus 5X)",  # Nexus 5X on Nougat
        "ci/android-pie-arm64-rel",  # Pixel 1, 2 on Pie
    ],
    compilator = "android-arm64-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "android-arm64-rel-compilator",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "android-arm64-siso-rel",
    description_html = """\
This builder shadows android-arm64-rel builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating android-arm64-rel from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/android-arm64-rel"),
    try_settings = builder_config.try_settings(
        # TODO: b/294287964 - waiting test devices to be allocated to handle
        # extra traffic.
        is_compile_only = True,
    ),
    compilator = "android-arm64-siso-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 20,
    ),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "android-arm64-siso-rel-compilator",
    main_list_view = "try",
    siso_enabled = True,
)

# TODO(crbug.com/1367523): Reeanble this builder once the reboot issue is resolved.
# try_.builder(
#     name = "android-asan",
#     mirrors = ["ci/android-asan"],
#     reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
# )

try_.builder(
    name = "android-bfcache-rel",
    mirrors = [
        "ci/android-bfcache-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-binary-size",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    executable = "recipe:binary_size_trybot",
    builderless = not settings.is_main,
    cores = 16,
    ssd = True,
    main_list_view = "try",
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
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-arm64-rel",
    mirrors = ["ci/android-cronet-arm64-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-asan-arm-rel",
    mirrors = ["ci/android-cronet-asan-arm-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-mainline-clang-arm64-dbg",
    mirrors = ["ci/android-cronet-mainline-clang-arm64-dbg"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-mainline-clang-arm64-rel",
    mirrors = ["ci/android-cronet-mainline-clang-arm64-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-mainline-clang-x86-dbg",
    mirrors = ["ci/android-cronet-mainline-clang-x86-dbg"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-mainline-clang-x86-rel",
    mirrors = ["ci/android-cronet-mainline-clang-x86-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x64-rel",
    mirrors = ["ci/android-cronet-x64-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x64-dbg",
    mirrors = ["ci/android-cronet-arm64-dbg"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x64-dbg-12-tests",
    mirrors = [
        "ci/android-cronet-x64-dbg",
        "ci/android-cronet-x64-dbg-12-tests",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x64-dbg-13-tests",
    mirrors = [
        "ci/android-cronet-x64-dbg",
        "ci/android-cronet-x64-dbg-13-tests",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg",
    mirrors = [
        "ci/android-cronet-x86-dbg",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-rel",
    mirrors = ["ci/android-cronet-x86-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-10-tests",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-10-tests",
    ],
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
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-oreo-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-oreo-tests",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-pie-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-pie-tests",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-cronet-x86-dbg-nougat-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-nougat-tests",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-deterministic-dbg",
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "android-deterministic-rel",
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
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
    name = "android-nougat-x86-siso-rel",
    description_html = """\
This builder shadows android-nougat-x86-rel builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating android-nougat-x86-rel from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/android-nougat-x86-rel"),
    try_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    compilator = "android-nougat-x86-siso-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 20,
    ),
    use_java_coverage = True,
)

try_.compilator_builder(
    name = "android-nougat-x86-siso-rel-compilator",
    cores = 64,
    main_list_view = "try",
    siso_enabled = True,
)

try_.orchestrator_builder(
    name = "android-nougat-x86-rel",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/android-nougat-x86-rel",
    ],
    compilator = "android-nougat-x86-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
    use_java_coverage = True,
)

try_.compilator_builder(
    name = "android-nougat-x86-rel-compilator",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    cores = 64 if settings.is_main else 32,
    main_list_view = "try",
)

try_.builder(
    name = "android-oreo-arm64-dbg",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Oreo Phone Tester",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-oreo-x86-rel",
    mirrors = [
        "ci/android-oreo-x86-rel",
    ],
    coverage_test_types = ["unit", "overall"],
    use_java_coverage = True,
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
    name = "android-pie-x86-rel",
    mirrors = [
        "ci/android-pie-x86-rel",
    ],
)

try_.builder(
    name = "android-webview-10-x86-rel-tests",
    mirrors = [
        "ci/android-x86-rel",
        "ci/android-webview-10-x86-rel-tests",
    ],
)

try_.builder(
    name = "android-chrome-pie-x86-wpt-fyi-rel",
    mirrors = ["ci/android-chrome-pie-x86-wpt-fyi-rel"],
)

try_.builder(
    name = "android-chrome-pie-x86-wpt-android-specific",
    mirrors = ["ci/android-chrome-pie-x86-wpt-android-specific"],
)

try_.builder(
    name = "android-webview-12-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-webview-12-x64-dbg-tests",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-13-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-webview-13-x64-dbg-tests",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-pie-x86-wpt-fyi-rel",
    mirrors = ["ci/android-webview-pie-x86-wpt-fyi-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-nougat-arm64-dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView N (dbg)",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-oreo-arm64-dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView O (dbg)",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "android-webview-pie-arm64-dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView P (dbg)",
    ],
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
)

try_.builder(
    name = "android-arm64-all-targets-dbg",
    mirrors = [
        "ci/Android arm64 Builder All Targets (dbg)",
    ],
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
        "ci/Android arm64 Builder All Targets (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    builderless = not settings.is_main,
    cores = 32 if settings.is_main else 16,
    ssd = True,
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
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            # Inclusion filters.
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
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "tools/mb/mb_config_expectations/tryserver.chromium.android.json"),
            cq.location_filter(path_regexp = "ui/gl/.+"),

            # Exclusion filters.
            cq.location_filter(exclude = True, path_regexp = ".*\\.md"),
        ],
    ),
)

try_.gpu.optional_tests_builder(
    name = "gpu-fyi-cq-android-arm64",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    mirrors = [
        "ci/GPU FYI Android arm64 Builder",
        "ci/Android FYI Release (Pixel 6)",
    ],
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            # Inclusion filters.
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
            cq.location_filter(path_regexp = "testing/buildbot/chromium.gpu.fyi.json"),
            cq.location_filter(path_regexp = "testing/trigger_scripts/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/mediastream/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webcodecs/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgl/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "tools/mb/mb_config_expectations/tryserver.chromium.android.json"),
            cq.location_filter(path_regexp = "ui/gl/.+"),

            # Exclusion filters.
            cq.location_filter(exclude = True, path_regexp = ".*\\.md"),
        ],
    ),
)

try_.builder(
    name = "android-code-coverage",
    mirrors = ["ci/android-code-coverage"],
    execution_timeout = 20 * time.hour,
)

try_.builder(
    name = "android-code-coverage-native",
    mirrors = ["ci/android-code-coverage-native"],
    execution_timeout = 20 * time.hour,
)
