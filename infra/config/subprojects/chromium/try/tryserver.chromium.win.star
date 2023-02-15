# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.win builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.win",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    cores = 8,
    os = os.WINDOWS_DEFAULT,
    compilator_cores = 32,
    compilator_goma_jobs = goma.jobs.J300,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    orchestrator_cores = 2,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.win",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
)

try_.builder(
    name = "win-annotator-rel",
)

try_.builder(
    name = "win-asan",
    execution_timeout = 5 * time.hour,
    goma_jobs = goma.jobs.J150,
)

try_.builder(
    name = "win10-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    goma_jobs = goma.jobs.J150,
)

try_.builder(
    name = "win-celab-try-rel",
    executable = "recipe:celab",
    properties = {
        "exclude": "chrome_only",
        "pool_name": "celab-chromium-try",
        "pool_size": 20,
        "tests": "*",
    },
)

try_.builder(
    name = "win-libfuzzer-asan-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    executable = "recipe:chromium_libfuzzer_trybot",
    builderless = False,
    os = os.WINDOWS_ANY,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "win_archive",
)

try_.builder(
    name = "win_chromium_compile_dbg_ng",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Win Builder (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    builderless = False,
    cores = 16,
    ssd = True,
    goma_jobs = goma.jobs.J150,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "win_chromium_compile_rel_ng",
    mirrors = [
        "ci/Win Builder",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
)

try_.builder(
    name = "win_chromium_dbg_ng",
)

try_.builder(
    name = "win_chromium_x64_rel_ng",
    mirrors = [
        "ci/Win x64 Builder",
        "ci/Win 7 Tests x64 (1)",
    ],
)

try_.builder(
    name = "win_upload_clang",
    executable = "recipe:chromium_upload_clang",
    builderless = False,
    cores = 32,
    os = os.WINDOWS_ANY,
    execution_timeout = 6 * time.hour,
    goma_backend = None,
)

try_.builder(
    name = "win_x64_archive",
)

try_.builder(
    name = "win10_chromium_x64_dbg_ng",
    os = os.WINDOWS_10,
)

try_.builder(
    name = "win11-x64-fyi-rel",
    mirrors = [
        "ci/Win x64 Builder",
        "ci/Win11 Tests x64",
    ],
    builderless = True,
    os = os.WINDOWS_10,
    coverage_test_types = ["unit", "overall"],
    use_clang_coverage = True,
)

try_.builder(
    name = "win10_chromium_inverse_fieldtrials_x64_fyi_rel_ng",
    mirrors = [
        "ci/Win x64 Builder",
        "ci/Win10 Tests x64",
        "ci/GPU Win x64 Builder",
        "ci/Win10 x64 Release (NVIDIA)",
    ],
    os = os.WINDOWS_10,
)

try_.orchestrator_builder(
    name = "win10_chromium_x64_rel_ng",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Win x64 Builder",
        "ci/Win10 Tests x64",
        "ci/GPU Win x64 Builder",
        "ci/Win10 x64 Release (NVIDIA)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    compilator = "win10_chromium_x64_rel_ng-compilator",
    coverage_test_types = ["unit", "overall"],
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "win10_chromium_x64_rel_ng-compilator",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    # TODO (crbug.com/1245171): Revert when root issue is fixed
    grace_period = 4 * time.minute,
    main_list_view = "try",
)

try_.builder(
    name = "win10_chromium_x64_rel_ng_exp",
    builderless = False,
    os = os.WINDOWS_ANY,
)

try_.builder(
    name = "win7-rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    mirrors = [
        "ci/Win Builder",
        "ci/Win7 Tests (1)",
    ],
    cores = 16,
    ssd = True,
    execution_timeout = 4 * time.hour + 30 * time.minute,
    goma_jobs = goma.jobs.J300,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "sandbox/win/.+",
            "sandbox/policy/win/.+",
        ],
    ),
)

try_.gpu.optional_tests_builder(
    name = "win_optional_gpu_tests_rel",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
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
    try_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/browser/vr/.+",
            "content/browser/xr/.+",
            "content/test/gpu/.+",
            "device/vr/.+",
            "gpu/.+",
            "media/audio/.+",
            "media/base/.+",
            "media/capture/.+",
            "media/filters/.+",
            "media/gpu/.+",
            "media/mojo/.+",
            "media/renderers/.+",
            "media/video/.+",
            "testing/buildbot/chromium.gpu.fyi.json",
            "testing/trigger_scripts/.+",
            "third_party/blink/renderer/modules/vr/.+",
            "third_party/blink/renderer/modules/mediastream/.+",
            "third_party/blink/renderer/modules/webcodecs/.+",
            "third_party/blink/renderer/modules/webgl/.+",
            "third_party/blink/renderer/modules/xr/.+",
            "third_party/blink/renderer/platform/graphics/gpu/.+",
            "tools/clang/scripts/update.py",
            "ui/gl/.+",
        ],
    ),
)
