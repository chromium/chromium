# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.linux builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "os", "reclient", "siso")
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
    compilator_reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 2,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_configs = ["builder"],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
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
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "leak_detection_linux",
    mirrors = ["ci/Leak Detection Linux"],
)

try_.builder(
    name = "linux-afl-asan-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    executable = "recipe:chromium/fuzz",
)

try_.builder(
    name = "linux-annotator-rel",
    mirrors = ["ci/linux-annotator-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-arm64-castos",
    mirrors = [
        "ci/Cast Linux ARM64",
    ],
    main_list_view = "try",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
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
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-blink-heap-verification-try",
    mirrors = ["ci/linux-blink-heap-verification"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-centipede-asan-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    executable = "recipe:chromium/fuzz",
)

try_.builder(
    name = "linux-clobber-rel",
    mirrors = [
        "ci/linux-archive-rel",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-dcheck-off-rel",
    mirrors = builder_config.copy_from("linux-rel"),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

# TODO(crbug.com/1442587): Remove this builder after burning down failures
# found when we now post-process stdout.
try_.builder(
    name = "linux-exp-asan-lsan-fyi-rel",
    mirrors = [
        "ci/linux-exp-asan-lsan-fyi-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

# TODO(crbug.com/1442587): Remove this builder after burning down failures
# found when we now post-process stdout.
try_.builder(
    name = "linux-exp-msan-fyi-rel",
    mirrors = [
        "ci/linux-exp-msan-fyi-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

# TODO(crbug.com/1442587): Remove this builder after burning down failures
# found when we now post-process stdout.
try_.builder(
    name = "linux-exp-tsan-fyi-rel",
    mirrors = [
        "ci/linux-exp-tsan-fyi-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-extended-tracing-rel",
    mirrors = [
        "ci/linux-extended-tracing-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-gcc-rel",
    mirrors = [
        "ci/linux-gcc-rel",
    ],
    # Focal is needed for better C++20 support. See crbug.com/1284275.
    os = os.LINUX_FOCAL,
)

try_.builder(
    name = "linux-headless-shell-rel",
    mirrors = ["ci/linux-headless-shell-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "components/headless/.+",
            "headless/.+",
        ],
    ),
)

try_.builder(
    name = "linux-inverse-fieldtrials-fyi-rel",
    mirrors = builder_config.copy_from("linux-rel"),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-fieldtrial-rel",
    mirrors = ["ci/linux-fieldtrial-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-mbi-mode-per-render-process-host-rel",
    mirrors = builder_config.copy_from("linux-rel"),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-mbi-mode-per-site-instance-rel",
    mirrors = builder_config.copy_from("linux-rel"),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-lacros-fyi-rel",
    mirrors = [
        "ci/linux-lacros-builder-fyi-rel",
        "ci/linux-lacros-tester-fyi-rel",
    ],
)

try_.builder(
    name = "linux-lacros-version-skew-fyi",
    mirrors = ["ci/linux-lacros-version-skew-fyi"],
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
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-libfuzzer-asan-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    executable = "recipe:chromium/fuzz",
    builderless = not settings.is_main,
    main_list_view = "try",
    tryjob = try_.job(),
)

try_.builder(
    name = "linux-perfetto-rel",
    mirrors = ["ci/linux-perfetto-rel"],
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
    compilator = "linux-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
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
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "linux-siso-rel",
    description_html = """\
This builder shadows linux-rel builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating linux-rel from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/linux-rel"),
    try_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    compilator = "linux-siso-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 10,
    ),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "linux-siso-rel-compilator",
    main_list_view = "try",
    siso_enabled = True,
)

# TODO(crbug.com/1394755): Remove this builder after burning down failures
# and measuring performance to see if we can roll UBSan into ASan.
try_.builder(
    name = "linux-ubsan-fyi-rel",
    mirrors = [
        "ci/linux-ubsan-fyi-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "linux-wayland-rel",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux Builder (Wayland)",
        "ci/Linux Tests (Wayland)",
    ],
    compilator = "linux-wayland-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "linux-wayland-rel-compilator",
    branch_selector = branches.selector.LINUX_BRANCHES,
    ssd = True,
    main_list_view = "try",
    siso_enabled = True,
)

try_.orchestrator_builder(
    name = "linux-wayland-siso-rel",
    description_html = """\
This builder shadows linux-wayland-rel builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating linux-wayland-rel from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/linux-wayland-rel"),
    try_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    compilator = "linux-wayland-siso-rel-compilator",
    coverage_test_types = ["unit", "overall"],
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 10,
    ),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "linux-wayland-siso-rel-compilator",
    ssd = True,
    main_list_view = "try",
    siso_enabled = True,
)

try_.builder(
    name = "linux-viz-rel",
    mirrors = ["ci/Linux Viz"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-webkit-asan-rel",
    mirrors = [
        "ci/WebKit Linux ASAN",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-webkit-msan-rel",
    mirrors = [
        "ci/WebKit Linux MSAN",
    ],
    # At this time, MSan is only compatibly with Focal. See
    # //docs/linux/instrumented_libraries.md.
    os = os.LINUX_FOCAL,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/linux-wpt-content-shell-fyi-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-wpt-content-shell-leak-detection",
    mirrors = [
        "ci/linux-wpt-content-shell-leak-detection",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-wpt-fyi-rel",
    mirrors = ["ci/linux-wpt-fyi-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-wpt-identity-fyi-rel",
    mirrors = ["ci/linux-wpt-identity-fyi-rel"],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-wpt-input-fyi-rel",
    mirrors = ["ci/linux-wpt-input-fyi-rel"],
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
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(),
)

try_.builder(
    name = "linux-x64-castos-audio",
    mirrors = [
        "ci/Cast Audio Linux",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-x64-castos-dbg",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Cast Linux Debug",
    ],
    main_list_view = "try",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
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
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "linux_chromium_asan_rel_ng",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux ASan LSan Builder",
        "ci/Linux ASan LSan Tests (1)",
    ],
    compilator = "linux_chromium_asan_rel_ng-compilator",
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux_chromium_asan_rel_ng-compilator",
    branch_selector = branches.selector.LINUX_BRANCHES,
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "linux_chromium_asan_siso_rel_ng",
    description_html = """\
This builder shadows linux_chromium_asan_rel_ng builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating linux_chromium_asan_rel_ng from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/linux_chromium_asan_rel_ng"),
    try_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    compilator = "linux_chromium_asan_siso_rel_ng-compilator",
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 10,
    ),
)

try_.compilator_builder(
    name = "linux_chromium_asan_siso_rel_ng-compilator",
    main_list_view = "try",
    siso_enabled = True,
)

try_.builder(
    name = "linux_chromium_cfi_rel_ng",
    mirrors = [
        "ci/Linux CFI",
    ],
    cores = 32,
    ssd = True,
    # TODO(thakis): Remove once https://crbug.com/927738 is resolved.
    execution_timeout = 7 * time.hour,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            # Diectories that have caused breakages in the past due to the
            # TensorFlowLite roll.
            "third_party/eigen3/.+",
            "third_party/farmhash/.+",
            "third_party/fft2d/.+",
            "third_party/flatbuffers/.+",
            "third_party/fp16/.+",
            "third_party/fxdiv/.+",
            "third_party/gemmlowp/.+",
            "third_party/pthreadpool/.+",
            "third_party/ruy/.+",
            "third_party/tflite/.+",
            "third_party/xnnpack/.+",
        ],
    ),
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
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_chromeos_msan_rel_ng",
    mirrors = [
        "ci/Linux ChromiumOS MSan Builder",
        "ci/Linux ChromiumOS MSan Tests",
    ],
    cores = 16,
    # At this time, MSan is only compatibly with Focal. See
    # //docs/linux/instrumented_libraries.md.
    os = os.LINUX_FOCAL,
    ssd = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_chromium_clobber_deterministic",
    executable = "recipe:swarming/deterministic_build",
    execution_timeout = 6 * time.hour,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
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
    # At this time, MSan is only compatibly with Focal. See
    # //docs/linux/instrumented_libraries.md.
    os = os.LINUX_FOCAL,
    execution_timeout = 6 * time.hour,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "linux_chromium_tsan_rel_ng",
    branch_selector = branches.selector.LINUX_BRANCHES,
    mirrors = [
        "ci/Linux TSan Builder",
        "ci/Linux TSan Tests",
    ],
    compilator = "linux_chromium_tsan_rel_ng-compilator",
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    # use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "linux_chromium_tsan_rel_ng-compilator",
    branch_selector = branches.selector.LINUX_BRANCHES,
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "linux_chromium_tsan_siso_rel_ng",
    description_html = """\
This builder shadows linux_chromium_tsan_rel_ng builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating linux_chromium_tsan_rel_ng from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/linux_chromium_tsan_rel_ng"),
    try_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    compilator = "linux_chromium_tsan_siso_rel_ng-compilator",
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 10,
    ),
)

try_.compilator_builder(
    name = "linux_chromium_tsan_siso_rel_ng-compilator",
    main_list_view = "try",
    siso_enabled = True,
)

try_.builder(
    name = "linux-ubsan-vptr",
    mirrors = [
        "ci/linux-ubsan-vptr",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux-lacros-asan-lsan-rel",
    mirrors = [
        "ci/linux-lacros-asan-lsan-rel",
    ],
    cores = 16,
    ssd = True,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "linux_upload_clang",
    executable = "recipe:chromium_toolchain/package_clang",
    builderless = True,
    cores = 32,
    ssd = True,
    execution_timeout = 5 * time.hour,
    notifies = ["chrome-rust-toolchain"],
)

try_.builder(
    name = "linux_upload_rust",
    executable = "recipe:chromium_toolchain/package_rust",
    builderless = True,
    cores = 32,
    ssd = True,
    execution_timeout = 5 * time.hour,
    notifies = ["chrome-rust-toolchain"],
)

try_.builder(
    name = "linux-v4l2-codec-rel",
    mirrors = [
        "ci/linux-v4l2-codec-rel",
    ],
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = "media/gpu/chromeos/.+"),
            cq.location_filter(path_regexp = "media/gpu/v4l2/.+"),
        ],
    ),
)

try_.builder(
    name = "network_service_linux",
    mirrors = [
        "ci/Network Service Linux",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
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
            # Inclusion filters.
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
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "tools/mb/mb_config_expectations/tryserver.chromium.linux.json"),
            cq.location_filter(path_regexp = "ui/gl/.+"),

            # Exclusion filters.
            cq.location_filter(exclude = True, path_regexp = ".*\\.md"),
        ],
    ),
)

# This builder is different from try/linux-js-code-coverage builder below as
# this is a try builder meant to provide javascript coverage for webui related
# CLs, where as try/linux-js-code-coverage builder is there to test changes in
# ci/linux-js-code-coverage builder and would mostly be used by coverage devs
# only.
try_.builder(
    name = "linux-js-coverage-rel",
    mirrors = ["ci/linux-js-code-coverage"],
    check_for_flakiness = False,
    check_for_flakiness_with_resultdb = False,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            cq.location_filter(path_regexp = r".*\.(js|ts)"),
        ],
    ),
    use_javascript_coverage = True,
)

# This builder is different from try/chromeos-js-code-coverage builder below as
# this is a try builder meant to provide javascript coverage for webui related
# CLs, where as try/chromeos-js-code-coverage builder is there to test changes
# in ci/chromeos-js-code-coverage builder and would mostly be used by coverage
# devs only.
try_.builder(
    name = "chromeos-js-coverage-rel",
    mirrors = ["ci/chromeos-js-code-coverage"],
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 20,
        location_filters = [
            cq.location_filter(path_regexp = r".*\.(js|ts)"),
        ],
    ),
    use_javascript_coverage = True,
)

# Coverage builders set up mainly to test changes in CI builders
try_.builder(
    name = "linux-code-coverage",
    mirrors = ["ci/linux-code-coverage"],
    execution_timeout = 20 * time.hour,
)

try_.builder(
    name = "linux-chromeos-code-coverage",
    mirrors = ["ci/linux-chromeos-code-coverage"],
    execution_timeout = 20 * time.hour,
)

try_.builder(
    name = "linux-lacros-code-coverage",
    mirrors = ["ci/linux-lacros-code-coverage"],
    execution_timeout = 20 * time.hour,
)

# This builder serves a different purpose than try/linux-js-coverage-rel
# See the note on linux-js-coverage-rel builder above to understand more.
try_.builder(
    name = "linux-js-code-coverage",
    mirrors = ["ci/linux-js-code-coverage"],
    execution_timeout = 20 * time.hour,
    use_javascript_coverage = True,
)

try_.builder(
    name = "chromeos-js-code-coverage",
    mirrors = ["ci/chromeos-js-code-coverage"],
    execution_timeout = 20 * time.hour,
    use_javascript_coverage = True,
)
############### Coverage Builders End ##################
