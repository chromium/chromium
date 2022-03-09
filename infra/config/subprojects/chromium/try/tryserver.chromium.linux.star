# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.linux builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "settings")

try_.defaults.set(
    builder_group = "tryserver.chromium.linux",
    cores = 8,
    orchestrator_cores = 2,
    compilator_cores = 16,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    compilator_goma_jobs = goma.jobs.J150,
    os = os.LINUX_BIONIC_SWITCH_TO_DEFAULT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.linux",
    branch_selector = [
        branches.CROS_LTS_MILESTONE,
        branches.FUCHSIA_LTS_MILESTONE,
    ],
)

try_.builder(
    name = "cast_shell_audio_linux",
)

try_.builder(
    name = "cast_shell_linux",
    branch_selector = branches.STANDARD_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "cast_shell_linux_dbg",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chromecast/.+",
        ],
    ),
)

try_.builder(
    name = "cast_shell_linux_arm64",
    branch_selector = branches.MAIN,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chromecast/.+",
        ],
    ),
    os = os.LINUX_BIONIC,
)

try_.builder(
    name = "fuchsia-binary-size",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    builderless = True,
    executable = "recipe:binary_size_fuchsia_trybot",
    properties = {
        "$build/binary_size": {
            "analyze_targets": [
                "//fuchsia/release:fuchsia_sizes",
            ],
            "compile_targets": [
                "fuchsia_sizes",
            ],
        },
    },
    tryjob = try_.job(
        experiment_percentage = 20,
    ),
)

try_.builder(
    name = "fuchsia-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    goma_jobs = goma.jobs.J150,
)

try_.builder(
    name = "fuchsia-arm64-cast",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chromecast/.+",
        ],
    ),
)

try_.builder(
    name = "fuchsia-compile-x64-dbg",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/base/fuchsia/.+",
            ".+/[+]/fuchsia/.+",
            ".+/[+]/media/fuchsia/.+",
        ],
    ),
)

try_.builder(
    name = "fuchsia-deterministic-dbg",
    executable = "recipe:swarming/deterministic_build",
)

try_.builder(
    name = "fuchsia-fyi-arm64-dbg",
)

try_.builder(
    name = "fuchsia-fyi-arm64-femu",
)

try_.builder(
    name = "fuchsia-fyi-arm64-rel",
)

try_.builder(
    name = "fuchsia-fyi-x64-dbg",
)

try_.builder(
    name = "fuchsia-fyi-x64-rel",
)

try_.builder(
    name = "fuchsia-x64-cast",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "fuchsia_arm64",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "fuchsia_x64",
    branch_selector = branches.FUCHSIA_LTS_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "layout_test_leak_detection",
)

try_.builder(
    name = "leak_detection_linux",
)

try_.builder(
    name = "linux-1mbu-compile-fyi-rel",
    builderless = False,
    goma_jobs = goma.jobs.J150,
    tryjob = try_.job(
        experiment_percentage = 5,
    ),
    properties = {
        "bot_update_experiments": [
            "no_sync",
        ],
    },
)

try_.builder(
    name = "linux-annotator-rel",
)

try_.builder(
    name = "linux-autofill-assistant",
)

try_.builder(
    name = "linux-bfcache-rel",
)

try_.builder(
    name = "linux-bionic-rel",
    goma_jobs = goma.jobs.J150,
    os = os.LINUX_BIONIC,
)

try_.builder(
    name = "linux-blink-heap-verification-try",
)

try_.builder(
    name = "linux-blink-web-tests-force-accessibility-rel",
)

try_.builder(
    name = "linux-clang-tidy-dbg",
    executable = "recipe:tricium_clang_tidy_wrapper",
    goma_jobs = goma.jobs.J150,
)

try_.builder(
    name = "linux-clang-tidy-rel",
    executable = "recipe:tricium_clang_tidy_wrapper",
    goma_jobs = goma.jobs.J150,
)

try_.builder(
    name = "linux-dcheck-off-rel",
)

try_.builder(
    name = "linux-example-builder",
)

try_.builder(
    name = "linux-extended-tracing-rel",
)

try_.builder(
    name = "linux-gcc-rel",
    goma_backend = None,
)

try_.builder(
    name = "linux-headless-shell-rel",
)

try_.builder(
    name = "linux-inverse-fieldtrials-fyi-rel",
)

try_.builder(
    name = "linux-fieldtrial-fyi-rel",
)

try_.builder(
    name = "linux-mbi-mode-per-render-process-host-rel",
)

try_.builder(
    name = "linux-mbi-mode-per-site-instance-rel",
)

try_.builder(
    name = "linux-lacros-fyi-rel",
)

try_.builder(
    name = "linux-lacros-version-skew-fyi",
)

try_.builder(
    name = "linux-layout-tests-edit-ng",
)

try_.builder(
    name = "linux-libfuzzer-asan-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    builderless = not settings.is_main,
    executable = "recipe:chromium_libfuzzer_trybot",
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "linux-perfetto-rel",
    tryjob = try_.job(
        experiment_percentage = 100,
        location_regexp = [
            ".+/[+]/base/trace_event/.+",
            ".+/[+]/base/tracing/.+",
            ".+/[+]/components/tracing/.+",
            ".+/[+]/content/browser/tracing/.+",
            ".+/[+]/services/tracing/.+",
        ],
    ),
)

try_.orchestrator_builder(
    name = "linux-rel",
    compilator = "linux-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    use_clang_coverage = True,
    coverage_test_types = ["unit", "overall"],
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "linux-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
)

# crbug.com/1270571: Experimental bot to test pre-warming
try_.orchestrator_builder(
    name = "linux-rel-warmed",
    compilator = "linux-rel-warmed-compilator",
    main_list_view = "try",
    use_clang_coverage = True,
    coverage_test_types = ["unit", "overall"],
)

# crbug.com/1270571: Experimental bot to test pre-warming
try_.compilator_builder(
    name = "linux-rel-warmed-compilator",
    main_list_view = "try",
    builder_cache_name = "linux_rel_warmed_compilator_warmed_cache",
)

try_.builder(
    name = "linux-wayland-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "linux-trusty-rel",
    goma_jobs = goma.jobs.J150,
    os = os.LINUX_TRUSTY,
)

try_.builder(
    name = "linux-xenial-rel",
    goma_jobs = goma.jobs.J150,
    os = os.LINUX_XENIAL,
)

try_.builder(
    name = "linux-viz-rel",
)

try_.builder(
    name = "linux-webkit-msan-rel",
)

try_.builder(
    name = "linux-wpt-fyi-rel",
)

try_.builder(
    name = "linux-wpt-identity-fyi-rel",
)

try_.builder(
    name = "linux-wpt-input-fyi-rel",
)

try_.builder(
    name = "linux_chromium_analysis",
)

try_.builder(
    name = "linux_chromium_archive_rel_ng",
)

try_.orchestrator_builder(
    name = "linux_chromium_asan_rel_ng",
    compilator = "linux_chromium_asan_rel_ng-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "linux_chromium_asan_rel_ng-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
)

try_.builder(
    name = "linux_chromium_cfi_rel_ng",
    cores = 32,
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 7 * time.hour,
)

try_.builder(
    name = "linux_chromium_chromeos_asan_rel_ng",
    goma_jobs = goma.jobs.J150,
    # TODO(crbug/1144484): Remove this timeout once we figure out the
    # regression in compiler or toolchain.
    execution_timeout = 7 * time.hour,
)

try_.builder(
    name = "linux_chromium_chromeos_msan_rel_ng",
    goma_jobs = goma.jobs.J150,
)

try_.builder(
    name = "linux_chromium_clobber_deterministic",
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "linux_chromium_clobber_rel_ng",
)

try_.builder(
    name = "linux_chromium_compile_dbg_32_ng",
)

try_.builder(
    name = "linux_chromium_compile_dbg_ng",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = ["ci/Linux Builder (dbg)"],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    builderless = not settings.is_main,
    caches = [
        swarming.cache(
            name = "builder",
            path = "linux_debug",
        ),
    ],
    goma_jobs = goma.jobs.J150,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "linux_chromium_compile_rel_ng",
)

try_.builder(
    name = "linux_chromium_dbg_ng",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Linux Builder (dbg)",
        "Linux Tests (dbg)(1)",
    ],
    caches = [
        swarming.cache(
            name = "builder",
            path = "linux_debug",
        ),
    ],
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/build/.*check_gn_headers.*",
        ],
    ),
)

try_.builder(
    name = "linux_chromium_msan_rel_ng",
    execution_timeout = 6 * time.hour,
    goma_jobs = goma.jobs.J150,
)

try_.orchestrator_builder(
    name = "linux_chromium_tsan_rel_ng",
    compilator = "linux_chromium_tsan_rel_ng-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "linux_chromium_tsan_rel_ng-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
)

try_.builder(
    name = "linux_chromium_ubsan_rel_ng",
)

try_.builder(
    name = "linux_layout_tests_layout_ng_disabled",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/third_party/blink/renderer/core/editing/.+",
            ".+/[+]/third_party/blink/renderer/core/layout/.+",
            ".+/[+]/third_party/blink/renderer/core/paint/.+",
            ".+/[+]/third_party/blink/renderer/core/svg/.+",
            ".+/[+]/third_party/blink/renderer/platform/fonts/shaping/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/.+",
            ".+/[+]/third_party/blink/web_tests/.+",
        ],
    ),
)

try_.builder(
    name = "linux_mojo",
)

try_.builder(
    name = "linux_mojo_chromeos",
)

try_.builder(
    name = "linux_upload_clang",
    builderless = True,
    cores = 32,
    executable = "recipe:chromium_upload_clang",
    goma_backend = None,
    # This builder produces the clang binaries used on all builders. Since it
    # uses the system's sysroot when compiling, the builder needs to run on the
    # OS version that's the oldest used on any bot.
    # TODO(crbug.com/1199405): Move this to bionic once _all_ builders have
    # migrated.
    os = os.LINUX_TRUSTY,
)

try_.builder(
    name = "linux_vr",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(
        location_regexp = [
            ".+/[+]/chrome/browser/vr/.+",
            ".+/[+]/content/browser/xr/.+",
        ],
    ),
)

try_.builder(
    name = "network_service_linux",
)

try_.builder(
    name = "tricium-metrics-analysis",
    executable = "recipe:tricium_metrics",
)

try_.builder(
    name = "tricium-oilpan-analysis",
    executable = "recipe:tricium_oilpan",
)

try_.builder(
    name = "tricium-simple",
    executable = "recipe:tricium_simple",
)

try_.gpu.optional_tests_builder(
    name = "linux_optional_gpu_tests_rel",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
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
            ".+/[+]/testing/buildbot/chromium.gpu.fyi.json",
            ".+/[+]/testing/trigger_scripts/.+",
            ".+/[+]/third_party/blink/renderer/modules/mediastream/.+",
            ".+/[+]/third_party/blink/renderer/modules/webcodecs/.+",
            ".+/[+]/third_party/blink/renderer/modules/webgl/.+",
            ".+/[+]/third_party/blink/renderer/platform/graphics/gpu/.+",
            ".+/[+]/tools/clang/scripts/update.py",
            ".+/[+]/ui/gl/.+",
        ],
    ),
)
