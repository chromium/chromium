# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.mac builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "goma", "os", "reclient", "xcode")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.mac",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    os = os.MAC_ANY,
    ssd = True,
    compilator_goma_jobs = goma.jobs.J150,
    compilator_reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    orchestrator_cores = 2,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

def ios_builder(*, name, **kwargs):
    kwargs.setdefault("builderless", False)
    kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("ssd", None)
    kwargs.setdefault("xcode", xcode.x14main)
    return try_.builder(name = name, **kwargs)

consoles.list_view(
    name = "tryserver.chromium.mac",
    branch_selector = [
        branches.selector.MAC_BRANCHES,
        branches.selector.IOS_BRANCHES,
    ],
)

try_.builder(
    name = "mac-arm64-on-arm64-rel",
    mirrors = [
        "ci/mac-arm64-on-arm64-rel",
    ],
    builderless = False,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-osxbeta-rel",
    mirrors = [
        "ci/Mac Builder (dbg)",
        "ci/mac-osxbeta-rel",
    ],
    builderless = False,
    os = os.MAC_13,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

# This trybot mirrors the trybot mac-rel
try_.builder(
    name = "mac-inverse-fieldtrials-fyi-rel",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac12 Tests",
        "ci/GPU Mac Builder",
        "ci/Mac Release (Intel)",
        "ci/Mac Retina Release (AMD)",
    ],
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "mac-fieldtrial-tester",
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac-fieldtrial-tester",
    ],
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "mac-builder-next-rel",
    mirrors = ["ci/Mac Builder Next"],
    builderless = False,
    os = os.MAC_13,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-perfetto-rel",
    mirrors = [
        "ci/mac-perfetto-rel",
    ],
)

try_.orchestrator_builder(
    name = "mac-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac12 Tests",
        "ci/GPU Mac Builder",
        "ci/Mac Release (Intel)",
        # TODO(crbug.com/1380184) Once the GPU test capacity situation is
        # resolved, restore this mirror.
        # "ci/Mac Retina Release (AMD)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    check_for_flakiness = True,
    compilator = "mac-rel-compilator",
    coverage_test_types = ["overall", "unit"],
    experiments = {
        "chromium_rts.inverted_rts": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    #use_orchestrator_pool = True,
)

try_.orchestrator_builder(
    name = "mac-rel-inverse-fyi",
    mirrors = builder_config.copy_from("try/mac-rel"),
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    check_for_flakiness = True,
    compilator = "mac-rel-compilator",
    coverage_test_types = ["overall", "unit"],
    experiments = {
        "chromium_rts.inverted_rts": 100,
        "chromium_rts.inverted_rts_bail_early": 100,
    },
    main_list_view = "try",
    use_clang_coverage = True,
    use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "mac-rel-compilator",
    branch_selector = branches.selector.MAC_BRANCHES,
    os = os.MAC_DEFAULT,
    check_for_flakiness = True,
    main_list_view = "try",
)

try_.builder(
    name = "mac10.15-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac10.15-wpt-content-shell-fyi-rel",
    ],
)

try_.builder(
    name = "mac11-arm64-rel",
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac11-arm64-rel-tests",
    ],
    builderless = True,
    check_for_flakiness = True,
    goma_backend = None,
)

try_.builder(
    name = "mac11-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac11-wpt-content-shell-fyi-rel",
    ],
)

try_.orchestrator_builder(
    name = "mac12-arm64-rel",
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac12-arm64-rel-tests",
    ],
    check_for_flakiness = True,
    compilator = "mac12-arm64-rel-compilator",
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 100,
    ),
)

try_.compilator_builder(
    name = "mac12-arm64-rel-compilator",
    os = os.MAC_12,
    check_for_flakiness = True,
    # TODO (crbug.com/1245171): Revert when root issue is fixed
    grace_period = 4 * time.minute,
    main_list_view = "try",
)

try_.builder(
    name = "mac12-arm64-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac12-arm64-wpt-content-shell-fyi-rel",
    ],
)

try_.builder(
    name = "mac12-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac12-wpt-content-shell-fyi-rel",
    ],
)

# NOTE: the following trybots aren't sensitive to Mac version on which
# they are built, hence no additional dimension is specified.
# The 10.xx version translates to which bots will run isolated tests.
try_.builder(
    name = "mac_chromium_10.13_rel_ng",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac10.13 Tests",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_10.14_rel_ng",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac10.14 Tests",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_10.15_rel_ng",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac10.15 Tests",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_11.0_rel_ng",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac11 Tests",
    ],
    builderless = False,
    goma_backend = None,
)

try_.builder(
    name = "mac12-tests",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac12 Tests",
    ],
)

try_.builder(
    name = "mac_chromium_archive_rel_ng",
    mirrors = [
        "ci/mac-archive-rel",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_asan_rel_ng",
    mirrors = [
        "ci/Mac ASan 64 Builder",
        "ci/Mac ASan 64 Tests (1)",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_compile_dbg_ng",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    os = os.MAC_DEFAULT,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "mac_chromium_compile_rel_ng",
    mirrors = [
        "ci/Mac Builder",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_dbg_ng",
    mirrors = [
        "ci/Mac Builder (dbg)",
        "ci/Mac12 Tests (dbg)",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_upload_clang",
    executable = "recipe:chromium_upload_clang",
    builderless = False,
    execution_timeout = 6 * time.hour,
    goma_backend = None,  # Does not use Goma.
)

try_.builder(
    name = "mac_upload_clang_arm",
    executable = "recipe:chromium_upload_clang",
    builderless = False,
    execution_timeout = 6 * time.hour,
    goma_backend = None,  # Does not use Goma.
)

try_.builder(
    name = "mac-code-coverage",
    mirrors = ["ci/mac-code-coverage"],
    execution_timeout = 20 * time.hour,
)

ios_builder(
    name = "ios-asan",
    mirrors = [
        "ci/ios-asan",
    ],
    goma_backend = None,
)

ios_builder(
    name = "ios-catalyst",
    mirrors = [
        "ci/ios-catalyst",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

ios_builder(
    name = "ios-device",
    mirrors = [
        "ci/ios-device",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

ios_builder(
    name = "ios-fieldtrial-rel",
    mirrors = ["ci/ios-fieldtrial-rel"],
    builderless = True,
)

ios_builder(
    name = "ios-m1-simulator",
    mirrors = ["ci/ios-m1-simulator"],
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
)

ios_builder(
    name = "ios-m1-simulator-cronet",
    mirrors = ["ci/ios-m1-simulator-cronet"],
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "ios-simulator",
    branch_selector = branches.selector.IOS_BRANCHES,
    mirrors = [
        "ci/ios-simulator",
    ],
    # TODO (crbug.com/1372179): Move back to orchestrator bots once they can be
    # properly rate limited
    # use_orchestrator_pool = True,
    cores = 2,
    os = os.LINUX_DEFAULT,
    check_for_flakiness = True,
    compilator = "ios-simulator-compilator",
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "ios-simulator-compilator",
    branch_selector = branches.selector.IOS_BRANCHES,
    # Set builderless to False so that branch builders use builderful bots
    builderless = False,
    os = os.MAC_DEFAULT,
    ssd = None,
    check_for_flakiness = True,
    goma_backend = None,
    main_list_view = "try",
    xcode = xcode.x14main,
)

ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.selector.IOS_BRANCHES,
    mirrors = [
        "ci/ios-simulator-cronet",
    ],
    check_for_flakiness = True,
    goma_backend = None,
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "components/cronet/.+",
            "components/grpc_support/.+",
            "ios/.+",
            cq.location_filter(exclude = True, path_regexp = "components/cronet/android/.+"),
        ],
    ),
)

ios_builder(
    name = "ios-simulator-full-configs",
    branch_selector = branches.selector.IOS_BRANCHES,
    mirrors = [
        "ci/ios-simulator-full-configs",
    ],
    check_for_flakiness = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "ios/.+",
        ],
    ),
    use_clang_coverage = True,
)

ios_builder(
    name = "ios-simulator-inverse-fieldtrials-fyi",
    mirrors = builder_config.copy_from("try/ios-simulator"),
)

ios_builder(
    name = "ios-simulator-multi-window",
    mirrors = ["ci/ios-simulator-multi-window"],
)

ios_builder(
    name = "ios-simulator-noncq",
    mirrors = [
        "ci/ios-simulator-noncq",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "third_party/crashpad/crashpad/.+",
        ],
    ),
)

ios_builder(
    name = "ios15-beta-simulator",
    mirrors = ["ci/ios15-beta-simulator"],
    goma_backend = None,
)

ios_builder(
    name = "ios15-sdk-simulator",
    mirrors = ["ci/ios15-sdk-simulator"],
    os = os.MAC_12,
    goma_backend = None,
)

ios_builder(
    name = "ios16-beta-simulator",
    mirrors = [
        "ci/ios16-beta-simulator",
    ],
    os = os.MAC_DEFAULT,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

ios_builder(
    name = "ios16-sdk-simulator",
    mirrors = [
        "ci/ios16-sdk-simulator",
    ],
    os = os.MAC_DEFAULT,
    goma_backend = None,
    xcode = xcode.x14betabots,
)

ios_builder(
    name = "ios-simulator-code-coverage",
    mirrors = ["ci/ios-simulator-code-coverage"],
    execution_timeout = 20 * time.hour,
)

try_.gpu.optional_tests_builder(
    name = "mac_optional_gpu_tests_rel",
    branch_selector = branches.selector.IOS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_internal",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-gpu-fyi-archive",
    ),
    ssd = None,
    goma_backend = None,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "chrome/browser/vr/.+"),
            cq.location_filter(path_regexp = "content/browser/xr/.+"),
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
            cq.location_filter(path_regexp = "services/shape_detection/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/tryserver.chromium.mac.json"),
            cq.location_filter(path_regexp = "testing/trigger_scripts/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/mediastream/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webcodecs/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgl/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "tools/mb/mb_config_expectations/tryserver.chromium.mac.json"),
            cq.location_filter(path_regexp = "ui/gl/.+"),
        ],
    ),
)

# RTS builders (https://crbug.com/1203048)

ios_builder(
    name = "ios-simulator-rts",
    mirrors = builder_config.copy_from("try/ios-simulator"),
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.ALWAYS,
        ),
    ),
    builderless = False,
    check_for_flakiness = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    use_clang_coverage = True,
)
