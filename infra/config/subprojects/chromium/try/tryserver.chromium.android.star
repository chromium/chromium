# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.android builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "settings")

try_.defaults.set(
    builder_group = "tryserver.chromium.android",
    cores = 8,
    compilator_cores = 32,
    orchestrator_cores = 4,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    compilator_goma_jobs = goma.jobs.J300,
    os = os.LINUX_DEFAULT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.android",
    branch_selector = branches.STANDARD_MILESTONE,
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
)

try_.builder(
    name = "android-12-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-12-x64-dbg-tests",
    ],
)

try_.orchestrator_builder(
    name = "android-12-x64-rel",
    compilator = "android-12-x64-rel-compilator",
    mirrors = [
        "ci/android-12-x64-rel",
    ],
    # TODO(crbug.com/1225851): Enable it on branch after running on CQ
    # branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 100,
    ),
)

try_.compilator_builder(
    name = "android-12-x64-rel-compilator",
    # TODO(crbug.com/1225851): Enable it on branch after running on CQ
    # branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
)

try_.builder(
    name = "android-12l-x86-rel",
    mirrors = [
        "ci/android-12l-x86-rel",
    ],
    tryjob = try_.job(
        experiment_percentage = 2,
    ),
)

try_.builder(
    name = "android-asan",
)

try_.builder(
    name = "android-bfcache-rel",
    mirrors = [
        "ci/android-bfcache-rel",
    ],
)

try_.builder(
    name = "android-binary-size",
    branch_selector = branches.STANDARD_MILESTONE,
    builderless = not settings.is_main,
    # TODO (kimstephanie): Change to cores = 16 and ssd = True once bots have
    # landed
    cores = 16,
    executable = "recipe:binary_size_trybot",
    goma_jobs = goma.jobs.J150,
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
    experiments = {
        "enable_weetbix_queries": 100,
    },
    tryjob = try_.job(),
    ssd = True,
)

try_.builder(
    name = "android-cronet-arm-dbg",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/android-cronet-arm-dbg",
    ],
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/components/cronet/.+",
            ".+/[+]/components/grpc_support/.+",
            ".+/[+]/build/android/.+",
            ".+/[+]/build/config/android/.+",
        ],
        location_regexp_exclude = [
            ".+/[+]/components/cronet/ios/.+",
        ],
    ),
)

try_.builder(
    name = "android-cronet-arm64-dbg",
)

try_.builder(
    name = "android-cronet-arm64-rel",
)

try_.builder(
    name = "android-cronet-asan-arm-rel",
)

try_.builder(
    name = "android-cronet-x86-dbg",
    mirrors = [
        "ci/android-cronet-x86-dbg",
    ],
)

try_.builder(
    name = "android-cronet-x86-rel",
)

try_.builder(
    name = "android-cronet-x86-dbg-10-tests",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-10-tests",
    ],
    check_for_flakiness = True,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/components/cronet/.+",
            ".+/[+]/components/grpc_support/.+",
            ".+/[+]/build/android/.+",
            ".+/[+]/build/config/android/.+",
        ],
        location_regexp_exclude = [
            ".+/[+]/components/cronet/ios/.+",
        ],
    ),
)

try_.builder(
    name = "android-cronet-x86-dbg-11-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-11-tests",
    ],
)

try_.builder(
    name = "android-cronet-x86-dbg-oreo-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-oreo-tests",
    ],
)

try_.builder(
    name = "android-cronet-x86-dbg-pie-tests",
    mirrors = [
        "ci/android-cronet-x86-dbg",
        "ci/android-cronet-x86-dbg-pie-tests",
    ],
)

try_.builder(
    name = "android-cronet-x86-rel-kitkat-tests",
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

try_.builder(
    name = "android-marshmallow-x86-fyi-rel-reviver",
)

try_.orchestrator_builder(
    name = "android-marshmallow-arm64-rel",
    mirrors = [
        "ci/android-marshmallow-arm64-rel",
        "ci/Android Release (Nexus 5X)",
    ],
    check_for_flakiness = True,
    compilator = "android-marshmallow-arm64-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    use_java_coverage = True,
    coverage_test_types = ["unit", "overall"],
    tryjob = try_.job(),
    experiments = {
        "remove_src_checkout_experiment": 100,
        "enable_weetbix_queries": 100,
    },
)

try_.compilator_builder(
    name = "android-marshmallow-arm64-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    check_for_flakiness = True,
    cores = 64 if settings.is_main else 32,
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "android-marshmallow-x86-rel",
    mirrors = [
        "ci/android-marshmallow-x86-rel",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    check_for_flakiness = True,
    compilator = "android-marshmallow-x86-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    use_java_coverage = True,
    coverage_test_types = ["unit", "overall"],
    tryjob = try_.job(),
    experiments = {
        "enable_weetbix_queries": 100,
    },
)

try_.compilator_builder(
    name = "android-marshmallow-x86-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    check_for_flakiness = True,
    main_list_view = "try",
)

try_.builder(
    name = "android-marshmallow-x86-rel-non-cq",
    mirrors = [
        "ci/android-marshmallow-x86-rel-non-cq",
    ],
)

try_.builder(
    name = "android-nougat-x86-rel",
    mirrors = ["ci/android-nougat-x86-rel"],
    tryjob = try_.job(
        experiment_percentage = 5,
    ),
)

try_.builder(
    name = "android-opus-arm-rel",
)

try_.builder(
    name = "android-oreo-arm64-cts-networkservice-dbg",
)

try_.builder(
    name = "android-oreo-arm64-dbg",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Oreo Phone Tester",
    ],
)

try_.builder(
    name = "android-pie-arm64-dbg",
    branch_selector = branches.STANDARD_MILESTONE,
    builderless = False,
    check_for_flakiness = True,
    cores = 16,
    goma_jobs = goma.jobs.J300,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/android/features/vr/.+",
            ".+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+",
            ".+/[+]/chrome/android/javatests/src/org/chromium/chrome/browser/vr/.+",
            ".+/[+]/chrome/browser/android/vr/.+",
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/device/vr/android/.+",
            ".+/[+]/third_party/gvr-android-sdk/.+",
            ".+/[+]/third_party/arcore-android-sdk/.+",
            ".+/[+]/third_party/arcore-android-sdk-client/.+",
        ],
    ),
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/android-pie-arm64-dbg",
    ],
)

# TODO(crbug/1182468) Remove when experiment is done.
try_.builder(
    name = "android-pie-arm64-coverage-experimental-rel",
    builderless = True,
    cores = 16,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    main_list_view = "try",
    use_clang_coverage = True,
    tryjob = try_.job(
        experiment_percentage = 3,
    ),
)

try_.orchestrator_builder(
    name = "android-pie-arm64-rel",
    mirrors = [
        "ci/android-pie-arm64-rel",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    compilator = "android-pie-arm64-rel-compilator",
    check_for_flakiness = True,
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(),
    experiments = {
        "enable_weetbix_queries": 100,
    },
)

try_.compilator_builder(
    name = "android-pie-arm64-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    check_for_flakiness = True,
    main_list_view = "try",
)

try_.builder(
    name = "android-pie-x86-rel",
    mirrors = [
        "ci/android-pie-x86-rel",
    ],
    goma_jobs = goma.jobs.J150,
)

try_.builder(
    name = "android-pie-x86-fyi-rel-reviver",
    mirrors = [
        "ci/android-pie-x86-fyi-rel-reviver",
    ],
    goma_jobs = goma.jobs.J150,
)

# TODO(crbug/1182468) Remove when coverage is enabled on CQ.
try_.builder(
    name = "android-pie-arm64-coverage-rel",
    cores = 16,
    goma_jobs = goma.jobs.J300,
    ssd = True,
    use_clang_coverage = True,
)

try_.builder(
    name = "android-webview-10-x86-rel-tests",
    mirrors = [
        "ci/android-x86-rel",
        "ci/android-webview-10-x86-rel-tests",
    ],
)

try_.builder(
    name = "android-pie-arm64-wpt-rel-non-cq",
)

try_.builder(
    name = "android-chrome-pie-x86-wpt-fyi-rel",
)

try_.builder(
    name = "android-weblayer-10-x86-rel-tests",
    mirrors = [
        "ci/android-weblayer-with-aosp-webview-x86-rel",
        "ci/android-weblayer-10-x86-rel-tests",
    ],
)

try_.builder(
    name = "android-weblayer-marshmallow-x86-rel-tests",
    mirrors = [
        "ci/android-weblayer-with-aosp-webview-x86-rel",
        "ci/android-weblayer-marshmallow-x86-rel-tests",
    ],
)

try_.builder(
    name = "android-weblayer-pie-x86-rel-tests",
    mirrors = [
        "ci/android-weblayer-x86-rel",
        "ci/android-weblayer-pie-x86-rel-tests",
    ],
)

try_.builder(
    name = "android-weblayer-pie-x86-wpt-fyi-rel",
)

try_.builder(
    name = "android-weblayer-pie-x86-wpt-smoketest",
)

try_.builder(
    name = "android-webview-12-x64-dbg",
    mirrors = [
        "ci/Android x64 Builder (dbg)",
        "ci/android-webview-12-x64-dbg-tests",
    ],
)

try_.builder(
    name = "android-webview-pie-x86-wpt-fyi-rel",
)

try_.builder(
    name = "android-webview-marshmallow-arm64-dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView M (dbg)",
    ],
)

try_.builder(
    name = "android-webview-nougat-arm64-dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView N (dbg)",
    ],
)

try_.builder(
    name = "android-webview-oreo-arm64-dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView O (dbg)",
    ],
)

try_.builder(
    name = "android-webview-pie-arm64-dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView P (dbg)",
    ],
)

try_.builder(
    name = "android-webview-pie-arm64-fyi-rel",
)

try_.builder(
    name = "android_archive_rel_ng",
    mirrors = [
        "ci/android-archive-rel",
    ],
)

try_.builder(
    name = "android_arm64_dbg_recipe",
    goma_jobs = goma.jobs.J300,
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
    goma_jobs = goma.jobs.J300,
    mirrors = [
        "ci/Android arm64 Builder All Targets (dbg)",
    ],
)

try_.builder(
    name = "android_blink_rel",
)

try_.builder(
    name = "android_compile_dbg",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Android arm Builder (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    builderless = not settings.is_main,
    goma_jobs = goma.jobs.J150,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "android_compile_x64_dbg",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Android x64 Builder (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    cores = 16,
    ssd = True,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+",
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/sandbox/linux/seccomp-bpf/.+",
            ".+/[+]/sandbox/linux/seccomp-bpf-helpers/.+",
            ".+/[+]/sandbox/linux/system_headers/.+",
            ".+/[+]/sandbox/linux/tests/.+",
            ".+/[+]/third_party/gvr-android-sdk/.+",
        ],
    ),
)

try_.builder(
    name = "android_compile_x86_dbg",
    branch_selector = branches.STANDARD_MILESTONE,
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
        location_regexp = [
            ".+/[+]/chrome/android/java/src/org/chromium/chrome/browser/vr/.+",
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/sandbox/linux/seccomp-bpf/.+",
            ".+/[+]/sandbox/linux/seccomp-bpf-helpers/.+",
            ".+/[+]/sandbox/linux/system_headers/.+",
            ".+/[+]/sandbox/linux/tests/.+",
            ".+/[+]/third_party/gvr-android-sdk/.+",
        ],
    ),
)

try_.builder(
    name = "android_cronet",
    mirrors = [
        "ci/android-cronet-arm-rel",
    ],
    try_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    branch_selector = branches.STANDARD_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
    experiments = {
        "enable_weetbix_queries": 100,
    },
)

try_.builder(
    name = "android_n5x_swarming_dbg",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Marshmallow 64 bit Tester",
    ],
)

try_.builder(
    name = "android_unswarmed_pixel_aosp",
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Android WebView N (dbg)",
    ],
)

try_.builder(
    name = "cast_shell_android",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Cast Android (dbg)",
    ],
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
    experiments = {
        "enable_weetbix_queries": 100,
    },
)

try_.builder(
    name = "linux_android_dbg_ng",
)

try_.builder(
    name = "try-nougat-phone-tester",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Android arm64 Builder (dbg)",
        "ci/Nougat Phone Tester",
    ],
)

try_.gpu.optional_tests_builder(
    name = "android_optional_gpu_tests_rel",
    branch_selector = branches.STANDARD_MILESTONE,
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
    goma_jobs = goma.jobs.J150,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/cc/.+",
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/components/viz/.+",
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/media/audio/.+",
            ".+/[+]/media/base/.+",
            ".+/[+]/media/capture/.+",
            ".+/[+]/media/filters/.+",
            ".+/[+]/media/gpu/.+",
            ".+/[+]/media/mojo/.+",
            ".+/[+]/media/renderers/.+",
            ".+/[+]/media/video/.+",
            ".+/[+]/services/viz/.+",
            ".+/[+]/testing/buildbot/tryserver.chromium.android.json",
            ".+/[+]/testing/trigger_scripts/.+",
            ".+/[+]/third_party/blink/renderer/modules/mediastream/.+",
            ".+/[+]/third_party/blink/renderer/modules/webcodecs/.+",
            ".+/[+]/third_party/blink/renderer/modules/webgl/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/tools/mb/mb_config_expectations/tryserver.chromium.android.json",
            ".+/[+]/ui/gl/.+",
        ],
    ),
)
