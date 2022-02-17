# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.win builder group."""

load("//lib/branches.star", "branches")
load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    builder_group = "tryserver.chromium.win",
    builderless = True,
    cores = 8,
    orchestrator_cores = 2,
    compilator_cores = 32,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    compilator_goma_jobs = goma.jobs.J300,
    os = os.WINDOWS_DEFAULT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.win",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
)

try_.builder(
    name = "win-annotator-rel",
)

try_.builder(
    name = "win-asan",
    goma_jobs = goma.jobs.J150,
    execution_timeout = 5 * time.hour,
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
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    builderless = False,
    executable = "recipe:chromium_libfuzzer_trybot",
    main_list_view = "try",
    os = os.WINDOWS_ANY,
    tryjob = try_.job(),
)

try_.builder(
    name = "win_archive",
)

try_.builder(
    name = "win_chromium_compile_dbg_ng",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    goma_jobs = goma.jobs.J150,
    main_list_view = "try",
    tryjob = try_.job(),
    builderless = False,
    cores = 16,
    ssd = True,
)

try_.builder(
    name = "win_chromium_compile_rel_ng",
)

try_.builder(
    name = "win_chromium_dbg_ng",
)

try_.builder(
    name = "win_chromium_x64_rel_ng",
)

try_.builder(
    name = "win_mojo",
)

try_.builder(
    name = "win_upload_clang",
    builderless = False,
    cores = 32,
    executable = "recipe:chromium_upload_clang",
    goma_backend = None,
    os = os.WINDOWS_ANY,
    execution_timeout = 6 * time.hour,
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
    builderless = True,
    use_clang_coverage = True,
    coverage_test_types = ["unit", "overall"],
    os = os.WINDOWS_10,
)

try_.builder(
    name = "win10_chromium_inverse_fieldtrials_x64_fyi_rel_ng",
    os = os.WINDOWS_10,
)

try_.orchestrator_builder(
    name = "win10_chromium_x64_rel_ng",
    compilator = "win10_chromium_x64_rel_ng-compilator",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    use_clang_coverage = True,
    coverage_test_types = ["unit", "overall"],
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "win10_chromium_x64_rel_ng-compilator",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    main_list_view = "try",
    # TODO (crbug.com/1245171): Revert when root issue is fixed
    grace_period = 4 * time.minute,
)

try_.builder(
    name = "win10_chromium_x64_rel_ng_exp",
    builderless = False,
    os = os.WINDOWS_ANY,
)

try_.builder(
    name = "win7-rel",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    cores = 16,
    execution_timeout = 4 * time.hour + 30 * time.minute,
    goma_jobs = goma.jobs.J300,
    main_list_view = "try",
    ssd = True,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/sandbox/win/.+",
            ".+/[+]/sandbox/policy/win/.+",
        ],
    ),
)

try_.gpu.optional_tests_builder(
    name = "win_optional_gpu_tests_rel",
    branch_selector = branches.DESKTOP_EXTENDED_STABLE_MILESTONE,
    builderless = True,
    main_list_view = "try",
    os = os.WINDOWS_DEFAULT,
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
            ".+/[+]/content/test/gpu/.+",
            ".+/[+]/device/vr/.+",
            ".+/[+]/gpu/.+",
            ".+/[+]/media/audio/.+",
            ".+/[+]/media/base/.+",
            ".+/[+]/media/capture/.+",
            ".+/[+]/media/filters/.+",
            ".+/[+]/media/gpu/.+",
            ".+/[+]/media/mojo/.+",
            ".+/[+]/media/renderers/.+",
            ".+/[+]/media/video/.+",
            ".+/[+]/testing/buildbot/chromium.gpu.fyi.json",
            ".+/[+]/testing/trigger_scripts/.+",
            ".+/[+]/third_party/blink/renderer/modules/vr/.+",
            ".+/[+]/third_party/blink/renderer/modules/mediastream/.+",
            ".+/[+]/third_party/blink/renderer/modules/webcodecs/.+",
            ".+/[+]/third_party/blink/renderer/modules/webgl/.+",
            ".+/[+]/third_party/blink/renderer/modules/xr/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/.+",
        ],
    ),
)
