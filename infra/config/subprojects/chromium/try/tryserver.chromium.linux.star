# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.linux builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "goma", "os", "reclient")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "settings")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.linux",
    pool = try_.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    compilator_cores = 8,
    compilator_goma_jobs = goma.jobs.J300,
    compilator_reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    orchestrator_cores = 2,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "tryserver.chromium.linux",
    branch_selector = branches.selector.LINUX_BRANCHES,
)

try_.builder(
    name = "layout_test_leak_detection",
    mirrors = [
        "ci/WebKit Linux Leak",
    ],
)

try_.builder(
    name = "leak_detection_linux",
    mirrors = ["ci/Leak Detection Linux"],
    goma_backend = None,
)

try_.builder(
    name = "linux-1mbu-compile-fyi-rel",
    mirrors = [
        "ci/Linux Builder",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    builderless = False,
    goma_jobs = goma.jobs.J150,
    properties = {
        "bot_update_experiments": [
            "no_sync",
        ],
    },
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        experiment_percentage = 5,
    ),
)

try_.builder(
    name = "linux-annotator-rel",
    mirrors = ["ci/linux-annotator-rel"],
)

try_.builder(
    name = "linux-arm64-castos",
    mirrors = [
        "ci/Cast Linux ARM64",
    ],
    os = os.LINUX_BIONIC,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chromecast/.+",
        ],
    ),
)

try_.builder(
    name = "linux-bfcache-rel",
    mirrors = [
        "ci/linux-bfcache-rel",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-blink-heap-verification-try",
    mirrors = ["ci/linux-blink-heap-verification"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-dcheck-off-rel",
    mirrors = builder_config.copy_from("linux-rel"),
)

try_.builder(
    name = "linux-extended-tracing-rel",
    mirrors = [
        "ci/linux-extended-tracing-rel",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-gcc-rel",
    mirrors = [
        "ci/linux-gcc-rel",
    ],
    os = os.LINUX_FOCAL,
    goma_backend = None,
)

try_.builder(
    name = "linux-headless-shell-rel",
    mirrors = ["ci/linux-headless-shell-rel"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-inverse-fieldtrials-fyi-rel",
    mirrors = builder_config.copy_from("linux-rel"),
)

try_.builder(
    name = "linux-fieldtrial-rel",
    mirrors = ["ci/linux-fieldtrial-rel"],
)

try_.builder(
    name = "linux-mbi-mode-per-render-process-host-rel",
    mirrors = builder_config.copy_from("linux-rel"),
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-mbi-mode-per-site-instance-rel",
    mirrors = builder_config.copy_from("linux-rel"),
)

try_.builder(
    name = "linux-lacros-fyi-rel",
    mirrors = [
        "ci/linux-lacros-builder-fyi-rel",
        "ci/linux-lacros-tester-fyi-rel",
    ],
    goma_backend = None,
)

try_.builder(
    name = "linux-lacros-version-skew-fyi",
    mirrors = ["ci/linux-lacros-version-skew-fyi"],
    goma_backend = None,
)

try_.builder(
    name = "linux-layout-tests-edit-ng",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(config = "chromium"),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = ["mb"],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
)

try_.builder(
    name = "linux-libfuzzer-asan-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    executable = "recipe:chromium_libfuzzer_trybot",
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "linux-perfetto-rel",
    mirrors = ["ci/linux-perfetto-rel"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "base/trace_event/.+",
            "base/tracing/.+",
            "components/tracing/.+",
            "content/browser/tracing/.+",
            "services/tracing/.+",
        ],
    ),
)

try_.orchestrator_builder(
    name = "linux-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux Builder",
        "ci/Linux Tests",
        "ci/GPU Linux Builder",
        "ci/Linux Release (NVIDIA)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    check_for_flakiness = True,
    compilator = "linux-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        "chromium_rts.inverted_rts": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux-rel-compilator",
    branch_selector = branches.selector.LINUX_BRANCHES,
    check_for_flakiness = True,
    main_list_view = "try",
)

# TODO(crbug.com/1394755): Remove this builder after burning down failures
# and measuring performance to see if we can roll UBSan into ASan.
try_.builder(
    name = "linux-ubsan-fyi-rel",
    mirrors = [
        "ci/linux-ubsan-fyi-rel",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "linux-wayland-rel-inverse-fyi",
    mirrors = [
        "ci/Linux Builder (Wayland)",
        "ci/Linux Tests (Wayland)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    compilator = "linux-wayland-rel-compilator",
    experiments = {
        "chromium_rts.inverted_rts": 100,
        "chromium_rts.inverted_rts_bail_early": 100,
    },
    use_orchestrator_pool = True,
)

try_.orchestrator_builder(
    name = "linux-wayland-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux Builder (Wayland)",
        "ci/Linux Tests (Wayland)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    compilator = "linux-wayland-rel-compilator",
    experiments = {
        "chromium_rts.inverted_rts": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.compilator_builder(
    name = "linux-wayland-rel-compilator",
    branch_selector = branches.selector.LINUX_BRANCHES,
    ssd = True,
    main_list_view = "try",
)

try_.builder(
    name = "linux-viz-rel",
    mirrors = ["ci/Linux Viz"],
)

try_.builder(
    name = "linux-webkit-msan-rel",
    mirrors = [
        "ci/WebKit Linux MSAN",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/linux-wpt-content-shell-fyi-rel",
    ],
)

try_.builder(
    name = "linux-wpt-fyi-rel",
    mirrors = ["ci/linux-wpt-fyi-rel"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-wpt-identity-fyi-rel",
    mirrors = ["ci/linux-wpt-identity-fyi-rel"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-wpt-input-fyi-rel",
    mirrors = ["ci/linux-wpt-input-fyi-rel"],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-x64-castos",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Cast Linux",
    ],
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "linux-x64-castos-audio",
    mirrors = [
        "ci/Cast Audio Linux",
    ],
)

try_.builder(
    name = "linux-x64-castos-dbg",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Cast Linux Debug",
    ],
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chromecast/.+",
        ],
    ),
)

try_.builder(
    name = "linux_chromium_archive_rel_ng",
    mirrors = [
        "ci/linux-archive-rel",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "linux_chromium_asan_rel_ng",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux ASan LSan Builder",
        "ci/Linux ASan LSan Tests (1)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    compilator = "linux_chromium_asan_rel_ng-compilator",
    experiments = {
        "chromium_rts.inverted_rts": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.orchestrator_builder(
    name = "linux_chromium_asan_rel_ng-inverse-fyi",
    mirrors = [
        "ci/Linux ASan LSan Builder",
        "ci/Linux ASan LSan Tests (1)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    compilator = "linux_chromium_asan_rel_ng-compilator",
    experiments = {
        "chromium_rts.inverted_rts": 100,
        "chromium_rts.inverted_rts_bail_early": 100,
    },
    use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux_chromium_asan_rel_ng-compilator",
    branch_selector = branches.selector.LINUX_BRANCHES,
    main_list_view = "try",
)

try_.builder(
    name = "linux_chromium_cfi_rel_ng",
    mirrors = [
        "ci/Linux CFI",
    ],
    cores = 32,
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 7 * time.hour,
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_chromeos_asan_rel_ng",
    mirrors = [
        "ci/Linux Chromium OS ASan LSan Builder",
        "ci/Linux Chromium OS ASan LSan Tests (1)",
    ],
    ssd = True,
    # TODO(crbug/1144484): Remove this timeout once we figure out the
    # regression in compiler or toolchain.
    execution_timeout = 7 * time.hour,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_chromeos_msan_focal",
    mirrors = [
        "ci/Linux ChromiumOS MSan Focal",
    ],
    os = os.LINUX_FOCAL,
    execution_timeout = 16 * time.hour,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_chromeos_msan_rel_ng",
    mirrors = [
        "ci/Linux ChromiumOS MSan Builder",
        "ci/Linux ChromiumOS MSan Tests",
    ],
    cores = 16,
    ssd = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_clobber_deterministic",
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "linux_chromium_clobber_rel_ng",
    mirrors = [
        "ci/linux-archive-rel",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
)

try_.builder(
    name = "linux_chromium_compile_dbg_ng",
    branch_selector = branches.selector.LINUX_BRANCHES,
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
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(),
)

try_.builder(
    name = "linux_chromium_compile_rel_ng",
    mirrors = [
        "ci/Linux Builder",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_dbg_ng",
    branch_selector = branches.selector.LINUX_BRANCHES,
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
    goma_backend = None,
    main_list_view = "try",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "build/.*check_gn_headers.*",
        ],
    ),
)

try_.builder(
    name = "linux_chromium_msan_rel_ng",
    mirrors = [
        "ci/Linux MSan Builder",
        "ci/Linux MSan Tests",
    ],
    os = os.LINUX_FOCAL,
    execution_timeout = 6 * time.hour,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "linux_chromium_tsan_rel_ng",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux TSan Builder",
        "ci/Linux TSan Tests",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    compilator = "linux_chromium_tsan_rel_ng-compilator",
    experiments = {
        "chromium_rts.inverted_rts": 100,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.orchestrator_builder(
    name = "linux_chromium_tsan_rel_ng-inverse-fyi",
    mirrors = [
        "ci/Linux TSan Builder",
        "ci/Linux TSan Tests",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    compilator = "linux_chromium_tsan_rel_ng-compilator",
    experiments = {
        "chromium_rts.inverted_rts": 100,
        "chromium_rts.inverted_rts_bail_early": 100,
    },
    use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux_chromium_tsan_rel_ng-compilator",
    branch_selector = branches.selector.LINUX_BRANCHES,
    main_list_view = "try",
)

try_.builder(
    name = "linux_chromium_ubsan_rel_ng",
    mirrors = [
        "ci/linux-ubsan-vptr",
    ],
    goma_backend = None,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-lacros-asan-lsan-rel",
    mirrors = [
        "ci/linux-lacros-asan-lsan-rel",
    ],
    cores = 16,
    ssd = True,
    goma_backend = None,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_layout_tests_layout_ng_disabled",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "third_party/blink/renderer/core/editing/.+",
            "third_party/blink/renderer/core/layout/.+",
            "third_party/blink/renderer/core/paint/.+",
            "third_party/blink/renderer/core/svg/.+",
            "third_party/blink/renderer/platform/fonts/shaping/.+",
            "third_party/blink/renderer/platform/graphics/.+",
            "third_party/blink/web_tests/.+",
        ],
    ),
)

try_.builder(
    name = "linux_upload_clang",
    executable = "recipe:chromium_upload_clang",
    builderless = True,
    cores = 32,
    # This builder produces the clang binaries used on all builders. Since it
    # uses the system's sysroot when compiling, the builder needs to run on the
    # OS version that's the oldest used on any bot.
    os = os.LINUX_BIONIC,
    execution_timeout = 5 * time.hour,
    goma_backend = None,
    notifies = ["chrome-rust-toolchain"],
)

try_.builder(
    name = "linux_vr",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/VR Linux",
    ],
    goma_backend = None,
    main_list_view = "try",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "chrome/browser/vr/.+",
            "content/browser/xr/.+",
        ],
    ),
)

try_.builder(
    name = "network_service_linux",
    mirrors = [
        "ci/Network Service Linux",
    ],
)

try_.builder(
    name = "tricium-metrics-analysis",
    executable = "recipe:tricium_metrics",
    goma_backend = None,
)

try_.builder(
    name = "tricium-oilpan-analysis",
    executable = "recipe:tricium_oilpan",
    goma_backend = None,
)

try_.builder(
    name = "tricium-simple",
    executable = "recipe:tricium_simple",
    goma_backend = None,
)

try_.gpu.optional_tests_builder(
    name = "linux_optional_gpu_tests_rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
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
            cq.location_filter(path_regexp = "testing/buildbot/tryserver.chromium.linux.json"),
            cq.location_filter(path_regexp = "testing/trigger_scripts/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/mediastream/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webcodecs/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgl/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "tools/mb/mb_config_expectations/tryserver.chromium.linux.json"),
            cq.location_filter(path_regexp = "ui/gl/.+"),
        ],
    ),
)

# RTS builders
try_.orchestrator_builder(
    name = "linux-rel-inverse-fyi",
    mirrors = [
        "ci/Linux Builder",
        "ci/Linux Tests",
        "ci/GPU Linux Builder",
        "ci/Linux Release (NVIDIA)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    check_for_flakiness = True,
    compilator = "linux-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        "chromium_rts.inverted_rts": 100,
        "chromium_rts.inverted_rts_bail_early": 100,
    },
    use_clang_coverage = True,
    use_orchestrator_pool = True,
)

# ML experimental builder, modifies RTS itself to use a ml model
try_.builder(
    name = "linux-rel-ml",
    mirrors = builder_config.copy_from("linux-rel"),
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.ALWAYS,
        ),
    ),
    builderless = False,
    cores = 16,
    experiments = {"chromium_rts.experimental_model": 100},
    goma_backend = None,
    tryjob = try_.job(
        experiment_percentage = 5,
    ),
)
