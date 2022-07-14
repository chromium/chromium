# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.mac builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "goma", "os", "xcode")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.chromium.mac",
    builderless = True,
    orchestrator_cores = 2,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    compilator_goma_jobs = goma.jobs.J150,
    os = os.MAC_ANY,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    ssd = True,
)

def ios_builder(*, name, **kwargs):
    kwargs.setdefault("builderless", False)
    kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("ssd", None)
    kwargs.setdefault("xcode", xcode.x13main)
    return try_.builder(name = name, **kwargs)

consoles.list_view(
    name = "tryserver.chromium.mac",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

try_.builder(
    name = "mac-arm64-on-arm64-rel",
    mirrors = [
        "ci/mac-arm64-on-arm64-rel",
    ],
    builderless = False,
    cpu = cpu.ARM64,
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "mac-osxbeta-rel",
    mirrors = [
        "ci/Mac Builder",
        "ci/mac-osxbeta-rel",
    ],
    os = os.MAC_DEFAULT,
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
    name = "mac-fieldtrial-rel",
    os = os.MAC_DEFAULT,
    mirrors = ["ci/mac-fieldtrial-rel"],
)

try_.builder(
    name = "mac-builder-next-rel",
    os = os.MAC_12,
    builderless = False,
)

try_.orchestrator_builder(
    name = "mac-rel",
    compilator = "mac-rel-compilator",
    check_for_flakiness = True,
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac12 Tests",
        "ci/GPU Mac Builder",
        "ci/Mac Release (Intel)",
        "ci/Mac Retina Release (AMD)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    main_list_view = "try",
    use_clang_coverage = True,
    tryjob = try_.job(),
    experiments = {
        "remove_src_checkout_experiment": 100,
    },
    use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "mac-rel-compilator",
    check_for_flakiness = True,
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    main_list_view = "try",
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "mac11-arm64-rel",
    builderless = True,
    check_for_flakiness = True,
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac11-arm64-rel-tests",
    ],
)

try_.orchestrator_builder(
    name = "mac12-arm64-rel",
    check_for_flakiness = True,
    compilator = "mac12-arm64-rel-compilator",
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac12-arm64-rel-tests",
    ],
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 100,
    ),
)

try_.compilator_builder(
    name = "mac12-arm64-rel-compilator",
    check_for_flakiness = True,
    main_list_view = "try",
    os = os.MAC_12,
    # TODO (crbug.com/1245171): Revert when root issue is fixed
    grace_period = 4 * time.minute,
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
)

try_.builder(
    name = "mac_chromium_10.14_rel_ng",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac10.14 Tests",
    ],
)

try_.builder(
    name = "mac_chromium_10.15_rel_ng",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac10.15 Tests",
    ],
)

try_.builder(
    name = "mac_chromium_11.0_rel_ng",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac11 Tests",
    ],
    builderless = False,
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
)

try_.builder(
    name = "mac_chromium_asan_rel_ng",
    mirrors = [
        "ci/Mac ASan 64 Builder",
        "ci/Mac ASan 64 Tests (1)",
    ],
    goma_jobs = goma.jobs.J150,
)

try_.builder(
    name = "mac_chromium_compile_dbg_ng",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    mirrors = [
        "ci/Mac Builder (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    goma_jobs = goma.jobs.J150,
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
)

try_.builder(
    name = "mac_chromium_dbg_ng",
    mirrors = [
        "ci/Mac Builder (dbg)",
        "ci/Mac12 Tests (dbg)",
    ],
)

try_.builder(
    name = "mac_upload_clang",
    builderless = False,
    executable = "recipe:chromium_upload_clang",
    execution_timeout = 6 * time.hour,
    goma_backend = None,  # Does not use Goma.
)

try_.builder(
    name = "mac_upload_clang_arm",
    builderless = False,
    executable = "recipe:chromium_upload_clang",
    execution_timeout = 6 * time.hour,
    goma_backend = None,  # Does not use Goma.
)

ios_builder(
    name = "ios-asan",
    mirrors = [
        "ci/ios-asan",
    ],
)

ios_builder(
    name = "ios-catalyst",
    mirrors = [
        "ci/ios-catalyst",
    ],
)

ios_builder(
    name = "ios-device",
    mirrors = [
        "ci/ios-device",
    ],
)

ios_builder(
    name = "ios-fieldtrial-rel",
    builderless = True,
    mirrors = ["ci/ios-fieldtrial-rel"],
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
)

ios_builder(
    name = "ios-simulator",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/ios-simulator",
    ],
    check_for_flakiness = True,
    main_list_view = "try",
    use_clang_coverage = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    tryjob = try_.job(),
)

ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/ios-simulator-cronet",
    ],
    check_for_flakiness = True,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/components/cronet/.+",
            ".+/[+]/components/grpc_support/.+",
            ".+/[+]/ios/.+",
        ],
        location_regexp_exclude = [
            ".+/[+]/components/cronet/android/.+",
        ],
    ),
)

ios_builder(
    name = "ios-simulator-full-configs",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/ios-simulator-full-configs",
    ],
    check_for_flakiness = True,
    main_list_view = "try",
    use_clang_coverage = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/ios/.+",
        ],
    ),
)

ios_builder(
    name = "ios-simulator-inverse-fieldtrials-fyi",
    mirrors = builder_config.copy_from("try/ios-simulator"),
)

ios_builder(
    name = "ios-simulator-multi-window",
)

ios_builder(
    name = "ios-simulator-noncq",
    mirrors = [
        "ci/ios-simulator-noncq",
    ],
    xcode = xcode.x13main,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/third_party/crashpad/crashpad/.+",
        ],
    ),
)

ios_builder(
    name = "ios15-beta-simulator",
)

ios_builder(
    name = "ios15-sdk-simulator",
    xcode = xcode.x13betabots,
    os = os.MAC_12,
)

ios_builder(
    name = "ios16-beta-simulator",
    os = os.MAC_DEFAULT,
    mirrors = [
        "ci/ios16-beta-simulator",
    ],
)

ios_builder(
    name = "ios16-sdk-simulator",
    os = os.MAC_DEFAULT,
    mirrors = [
        "ci/ios16-sdk-simulator",
    ],
    xcode = xcode.x14betabots,
)

try_.gpu.optional_tests_builder(
    name = "mac_optional_gpu_tests_rel",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
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
    main_list_view = "try",
    ssd = None,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
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
            ".+/[+]/services/shape_detection/.+",
            ".+/[+]/testing/buildbot/tryserver.chromium.mac.json",
            ".+/[+]/testing/trigger_scripts/.+",
            ".+/[+]/third_party/blink/renderer/modules/mediastream/.+",
            ".+/[+]/third_party/blink/renderer/modules/webcodecs/.+",
            ".+/[+]/third_party/blink/renderer/modules/webgl/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/tools/mb/mb_config_expectations/tryserver.chromium.mac.json",
            ".+/[+]/ui/gl/.+",
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
    use_clang_coverage = True,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
)
