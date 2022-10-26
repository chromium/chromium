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
    builder_group = "tryserver.chromium.linux",
    cores = 8,
    orchestrator_cores = 2,
    compilator_cores = 16,
    executable = try_.DEFAULT_EXECUTABLE,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    goma_backend = goma.backend.RBE_PROD,
    compilator_goma_jobs = goma.jobs.J150,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    compilator_reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    os = os.LINUX_DEFAULT,
    pool = try_.DEFAULT_POOL,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,

    # TODO(crbug.com/1362440): remove this.
    omit_python2 = False,
)

consoles.list_view(
    name = "tryserver.chromium.linux",
    branch_selector = branches.CROS_LTS_MILESTONE,
)

try_.builder(
    name = "layout_test_leak_detection",
    mirrors = [
        "ci/WebKit Linux Leak",
    ],
)

try_.builder(
    name = "leak_detection_linux",
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
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
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
    name = "linux-arm64-castos",
    branch_selector = branches.MAIN,
    mirrors = [
        "ci/Cast Linux ARM64",
    ],
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chromecast/.+",
        ],
    ),
    os = os.LINUX_BIONIC,
)

try_.builder(
    name = "linux-autofill-assistant",
)

try_.builder(
    name = "linux-bfcache-rel",
    mirrors = [
        "ci/linux-bfcache-rel",
    ],
)

try_.builder(
    name = "linux-blink-heap-verification-try",
)

try_.builder(
    name = "linux-blink-v8-sandbox-future-rel",
    mirrors = ["ci/linux-blink-v8-sandbox-future-rel"],
)

try_.builder(
    name = "linux-dcheck-off-rel",
    mirrors = builder_config.copy_from("linux-rel"),
)

try_.builder(
    name = "linux-example-builder",
)

try_.builder(
    name = "linux-extended-tracing-rel",
    mirrors = [
        "ci/linux-extended-tracing-rel",
    ],
)

try_.builder(
    name = "linux-gcc-rel",
    mirrors = [
        "ci/linux-gcc-rel",
    ],
    goma_backend = None,
    os = os.LINUX_FOCAL,
)

try_.builder(
    name = "linux-headless-shell-rel",
)

try_.builder(
    name = "linux-inverse-fieldtrials-fyi-rel",
    mirrors = builder_config.copy_from("linux-rel"),
)

try_.builder(
    name = "linux-fieldtrial-rel",
)

try_.builder(
    name = "linux-mbi-mode-per-render-process-host-rel",
    mirrors = builder_config.copy_from("linux-rel"),
)

try_.builder(
    name = "linux-mbi-mode-per-site-instance-rel",
    mirrors = builder_config.copy_from("linux-rel"),
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
    compilator = "linux-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    # Disabling due to crbug.com/1359208
    # check_for_flakiness = True,
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
    main_list_view = "try",
    use_clang_coverage = True,
    coverage_test_types = ["unit", "overall"],
    tryjob = try_.job(),
    experiments = {
        "remove_src_checkout_experiment": 100,
    },
    # TODO(crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    check_for_flakiness = True,
    main_list_view = "try",
)

try_.builder(
    name = "linux-wayland-rel",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Linux Builder (Wayland)",
        "ci/Linux Tests (Wayland)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

# TODO (crbug.com/1287228): Remove when orchestrator is confirmed to work
try_.orchestrator_builder(
    name = "linux-wayland-rel-orchestrator",
    compilator = "linux-wayland-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/Linux Builder (Wayland)",
        "ci/Linux Tests (Wayland)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    main_list_view = "try",
    experiments = {
        "remove_src_checkout_experiment": 100,
    },
    use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux-wayland-rel-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    # TODO (crbug.com/1287228): Set correct values once bots are set up
    ssd = None,
    cores = None,
)

try_.builder(
    name = "linux-viz-rel",
)

try_.builder(
    name = "linux-webkit-msan-rel",
    mirrors = [
        "ci/WebKit Linux MSAN",
    ],
)

try_.builder(
    name = "linux-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/linux-wpt-content-shell-fyi-rel",
    ],
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
    name = "linux-x64-castos",
    branch_selector = branches.STANDARD_MILESTONE,
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
    branch_selector = branches.STANDARD_MILESTONE,
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
    name = "linux_chromium_analysis",
)

try_.builder(
    name = "linux_chromium_archive_rel_ng",
    mirrors = [
        "ci/linux-archive-rel",
    ],
)

try_.orchestrator_builder(
    name = "linux_chromium_asan_rel_ng",
    compilator = "linux_chromium_asan_rel_ng-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(),
    mirrors = [
        "ci/Linux ASan LSan Builder",
        "ci/Linux ASan LSan Tests (1)",
    ],
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.QUICK_RUN_ONLY,
        ),
    ),
    experiments = {
        "remove_src_checkout_experiment": 100,
    },
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux_chromium_asan_rel_ng-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
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
)

try_.builder(
    name = "linux_chromium_chromeos_asan_rel_ng",
    mirrors = [
        "ci/Linux Chromium OS ASan LSan Builder",
        "ci/Linux Chromium OS ASan LSan Tests (1)",
    ],
    goma_jobs = goma.jobs.J150,
    # TODO(crbug/1144484): Remove this timeout once we figure out the
    # regression in compiler or toolchain.
    execution_timeout = 7 * time.hour,
    ssd = True,
)

try_.builder(
    name = "linux_chromium_chromeos_msan_focal",
    mirrors = [
        "ci/Linux ChromiumOS MSan Focal",
    ],
    goma_jobs = goma.jobs.J150,
    os = os.LINUX_FOCAL,
    execution_timeout = 16 * time.hour,
)

try_.builder(
    name = "linux_chromium_chromeos_msan_rel_ng",
    mirrors = [
        "ci/Linux ChromiumOS MSan Builder",
        "ci/Linux ChromiumOS MSan Tests",
    ],
    goma_jobs = goma.jobs.J150,
    ssd = True,
    cores = 16,
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
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    main_list_view = "try",
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
        location_filters = [
            "build/.*check_gn_headers.*",
        ],
    ),
)

try_.builder(
    name = "linux_chromium_msan_focal",
    mirrors = [
        "ci/Linux MSan Focal",
    ],
    execution_timeout = 16 * time.hour,
    goma_jobs = goma.jobs.J150,
    os = os.LINUX_FOCAL,
)

try_.builder(
    name = "linux_chromium_msan_rel_ng",
    mirrors = [
        "ci/Linux MSan Builder",
        "ci/Linux MSan Tests",
    ],
    execution_timeout = 6 * time.hour,
    goma_jobs = goma.jobs.J150,
)

try_.orchestrator_builder(
    name = "linux_chromium_tsan_rel_ng",
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
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(),
    experiments = {
        "remove_src_checkout_experiment": 100,
    },
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux_chromium_tsan_rel_ng-compilator",
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
)

try_.builder(
    name = "linux_chromium_ubsan_rel_ng",
    mirrors = [
        "ci/linux-ubsan-vptr",
    ],
)

try_.builder(
    name = "linux-lacros-asan-lsan-rel",
    mirrors = [
        "ci/linux-lacros-asan-lsan-rel",
    ],
    goma_jobs = goma.jobs.J150,
)

try_.builder(
    name = "linux_layout_tests_layout_ng_disabled",
    branch_selector = branches.STANDARD_MILESTONE,
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
        test_results_config = builder_config.test_results_config(
            config = "staging_server",
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
    builderless = True,
    cores = 32,
    executable = "recipe:chromium_upload_clang",
    goma_backend = None,
    # This builder produces the clang binaries used on all builders. Since it
    # uses the system's sysroot when compiling, the builder needs to run on the
    # OS version that's the oldest used on any bot.
    os = os.LINUX_BIONIC,
    notifies = ["chrome-rust-toolchain"],
    execution_timeout = 5 * time.hour,
)

try_.builder(
    name = "linux_vr",
    branch_selector = branches.STANDARD_MILESTONE,
    mirrors = [
        "ci/VR Linux",
    ],
    main_list_view = "try",
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
    branch_selector = branches.STANDARD_MILESTONE,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            "chrome/browser/vr/.+",
            "content/browser/xr/.+",
            "content/test/gpu/.+",
            "gpu/.+",
            "media/audio/.+",
            "media/base/.+",
            "media/capture/.+",
            "media/filters/.+",
            "media/gpu/.+",
            "media/mojo/.+",
            "media/renderers/.+",
            "media/video/.+",
            "testing/buildbot/tryserver.chromium.linux.json",
            "testing/trigger_scripts/.+",
            "third_party/blink/renderer/modules/mediastream/.+",
            "third_party/blink/renderer/modules/webcodecs/.+",
            "third_party/blink/renderer/modules/webgl/.+",
            "third_party/blink/renderer/platform/graphics/gpu/.+",
            "tools/clang/scripts/update.py",
            "tools/mb/mb_config_expectations/tryserver.chromium.linux.json",
            "ui/gl/.+",
        ],
    ),
)

# RTS builders
try_.orchestrator_builder(
    name = "linux-rel-inverse-fyi",
    compilator = "linux-rel-compilator",
    # Disabling due to crbug.com/1359208
    # check_for_flakiness = True,
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
    use_clang_coverage = True,
    coverage_test_types = ["unit", "overall"],
    tryjob = try_.job(
        experiment_percentage = 25,
    ),
    experiments = {
        "remove_src_checkout_experiment": 100,
        "chromium_rts.inverted_rts": 100,
        "chromium_rts.inverted_rts_bail_early": 100,
    },
    use_orchestrator_pool = True,
)

# ML experimental builder, modifies RTS itself to use a ml model
try_.builder(
    name = "linux-rel-ml",
    mirrors = builder_config.copy_from("linux-rel"),
    tryjob = try_.job(
        experiment_percentage = 5,
    ),
    try_settings = builder_config.try_settings(
        rts_config = builder_config.rts_config(
            condition = builder_config.rts_condition.ALWAYS,
        ),
    ),
    cores = 16,
    builderless = False,
    experiments = {"chromium_rts.experimental_model": 100},
)
